#include "task.h"
#include "copy.h"
#include "../queue/queue.h"
#include "../queue/group.h"
#include <Zend/zend_execute.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_closures.h>
#include <Zend/zend_map_ptr.h>

TSRMLS_CACHE_EXTERN();

gcd_task *gcd_task_create(zend_op_array *oa, zval *argv, uint32_t argc)
{
    gcd_task *t = pemalloc(sizeof(gcd_task), 1);
    t->op_array = oa;
    if (argv) {
        ZVAL_COPY_VALUE(&t->argv, argv);
    } else {
        ZVAL_NULL(&t->argv);
    }
    t->argc = argc;
    t->queue = NULL;
    t->group = NULL;
    t->is_sync = 0;
    t->is_barrier = 0;
    pthread_mutex_init(&t->mutex, NULL);
    pthread_cond_init(&t->cond, NULL);
    t->done = 0;
    t->failed = 0;
    t->next = NULL;
    return t;
}

void gcd_task_execute(gcd_task *t)
{
    gcd_queue *q = t->queue;
    int gated = q && q->width > 1;
    if (gated) {
        pthread_mutex_lock(&q->width_mutex);
        if (t->is_barrier) {
            while (q->inflight > 0 || q->suspended) {
                pthread_cond_wait(&q->width_cond, &q->width_mutex);
            }
            q->barrier_active = 1;
        } else {
            while (q->inflight >= q->width || q->barrier_active || q->suspended) {
                pthread_cond_wait(&q->width_cond, &q->width_mutex);
            }
            q->inflight++;
        }
        pthread_mutex_unlock(&q->width_mutex);
    }

    /* request-local argv copies */
    zval *params = t->argc ? ecalloc(t->argc, sizeof(zval)) : NULL;
    if (params && Z_TYPE(t->argv) == IS_ARRAY) {
        uint32_t i = 0;
        zval *v;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL(t->argv), v) {
            if (i >= t->argc) break;
            gcd_zval_copy(&params[i], v, 0);
            i++;
        } ZEND_HASH_FOREACH_END();
    }

    zend_function func;
    memset(&func, 0, sizeof(func));
    func.type = ZEND_USER_FUNCTION;
    func.op_array = *t->op_array;
    void **cache = func.op_array.cache_size ? ecalloc(1, func.op_array.cache_size) : NULL;
    ZEND_MAP_PTR_INIT(func.op_array.run_time_cache, cache);
    /* Treat as a plain user function: ZEND_ACC_CLOSURE would make the executor
     * derive a closure object pointer from the op_array address (invalid here). */
    func.common.fn_flags &= ~ZEND_ACC_CLOSURE;

    zval retval;
    ZVAL_UNDEF(&retval);
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    memset(&fci, 0, sizeof(fci));
    memset(&fcc, 0, sizeof(fcc));
    fci.size = sizeof(fci);
    ZVAL_UNDEF(&fci.function_name);
    fci.retval = &retval;
    fci.param_count = t->argc;
    fci.params = params;
    fcc.function_handler = &func;

    zend_call_function(&fci, &fcc);
    if (EG(exception)) {
        t->failed = 1;
        zend_clear_exception();
    }
    zval_ptr_dtor(&retval);
    if (params) {
        for (uint32_t i = 0; i < t->argc; i++) zval_ptr_dtor(&params[i]);
        efree(params);
    }
    if (cache) efree(cache);

    if (gated) {
        pthread_mutex_lock(&q->width_mutex);
        if (t->is_barrier) {
            q->barrier_active = 0;
        } else {
            q->inflight--;
        }
        pthread_cond_broadcast(&q->width_cond);
        pthread_mutex_unlock(&q->width_mutex);
    }

    if (t->is_sync) {
        pthread_mutex_lock(&t->mutex);
        t->done = 1;
        pthread_cond_signal(&t->cond);
        pthread_mutex_unlock(&t->mutex);
    } else {
        if (t->group) gcd_group_leave(t->group);
        gcd_task_destroy(t);
    }
}

gcd_task *gcd_task_build(zval *closure, zval *argv)
{
    if (!instanceof_function(Z_OBJCE_P(closure), zend_ce_closure)) {
        zend_throw_exception(zend_ce_type_error, "task must be a Closure", 0);
        return NULL;
    }
    zend_op_array *src_oa = (zend_op_array *) ((char *) Z_OBJ_P(closure) + sizeof(zend_object));
    zend_op_array *oa = gcd_op_array_copy_persistent(src_oa);
    if (!oa) {
        zend_throw_exception(gcd_capture_ce,
            "closure captures variables or uses statics/try-catch (unsupported in v1)", 0);
        return NULL;
    }
    zval pargv;
    ZVAL_NULL(&pargv);
    uint32_t argc = 0;
    if (argv) {
        gcd_zval_copy_persistent(&pargv, argv);
        argc = zend_hash_num_elements(Z_ARRVAL_P(argv));
    }
    return gcd_task_create(oa, &pargv, argc);
}

void gcd_task_wait(gcd_task *t)
{
    pthread_mutex_lock(&t->mutex);
    while (!t->done) {
        pthread_cond_wait(&t->cond, &t->mutex);
    }
    pthread_mutex_unlock(&t->mutex);
}

void gcd_task_destroy(gcd_task *t)
{
    gcd_op_array_free_persistent(t->op_array);
    gcd_zval_free(&t->argv, 1);
    pthread_mutex_destroy(&t->mutex);
    pthread_cond_destroy(&t->cond);
    pefree(t, 1);
}