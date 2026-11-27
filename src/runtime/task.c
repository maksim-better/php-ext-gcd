#include "task.h"
#include "copy.h"
#include "error.h"
#include "thread_pool.h"
#include "../queue/queue.h"
#include "../queue/group.h"
#include <Zend/zend_execute.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_closures.h>
#include <Zend/zend_map_ptr.h>

TSRMLS_CACHE_EXTERN();

__thread gcd_queue *gcd_current_queue = NULL;

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
    t->owns_oa = 1;
    pthread_mutex_init(&t->mutex, NULL);
    pthread_cond_init(&t->cond, NULL);
    t->done = 0;
    t->failed = 0;
    t->next = NULL;
    return t;
}

int gcd_invoke(zend_op_array *oa, zval *argv, uint32_t argc)
{
    zval *params = argc ? ecalloc(argc, sizeof(zval)) : NULL;
    if (params && argv && Z_TYPE_P(argv) == IS_ARRAY) {
        uint32_t i = 0;
        zval *v;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(argv), v) {
            if (i >= argc) break;
            gcd_zval_copy(&params[i], v, 0);
            i++;
        } ZEND_HASH_FOREACH_END();
    }

    zend_function func;
    memset(&func, 0, sizeof(func));
    func.type = ZEND_USER_FUNCTION;
    func.op_array = *oa;
    void **cache = func.op_array.cache_size ? ecalloc(1, func.op_array.cache_size) : NULL;
    ZEND_MAP_PTR_INIT(func.op_array.run_time_cache, cache);

    /* Wrap as a fake closure (does not own/free the persistent op_array) so the
     * closure calling convention applies and exceptions return to the caller. */
    zval closure_zv;
    zend_create_fake_closure(&closure_zv, &func, NULL, NULL, NULL);

    zval retval;
    ZVAL_UNDEF(&retval);
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    memset(&fci, 0, sizeof(fci));
    memset(&fcc, 0, sizeof(fcc));
    fci.size = sizeof(fci);
    ZVAL_COPY_VALUE(&fci.function_name, &closure_zv);
    fci.retval = &retval;
    fci.param_count = argc;
    fci.params = params;
    zend_is_callable_ex(&closure_zv, NULL, 0, NULL, &fcc, NULL);

    /* Worker threads have no running PHP frame; give the call a caller frame so
     * a thrown exception returns here instead of being treated as uncaught. */
    zend_execute_data *prev_ex = EG(current_execute_data);
    zend_execute_data dummy_ex;
    memset(&dummy_ex, 0, sizeof(dummy_ex));
    dummy_ex.prev_execute_data = prev_ex;
    /* NOTE: dummy_ex.func stays NULL. Release builds return the exception to the
     * caller correctly; PHP *debug* builds assert in zend_fetch_debug_backtrace
     * on this frame (known debug-only artifact, see implement-log). */
    EG(current_execute_data) = &dummy_ex;

    zend_call_function(&fci, &fcc);
    int failed = EG(exception) != NULL;   /* caller handles/clears exception */

    EG(current_execute_data) = prev_ex;
    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&closure_zv);
    if (params) {
        for (uint32_t i = 0; i < argc; i++) zval_ptr_dtor(&params[i]);
        efree(params);
    }
    if (cache) efree(cache);
    return failed;
}

void gcd_task_execute(gcd_task *t)
{
    gcd_queue *q = t->queue;
    int gated = q && q->width > 1;
    if (gated) {
        pthread_mutex_lock(&q->width_mutex);
        if (t->is_barrier) {
            while (q->inflight > 0 || q->barrier_active || q->suspended) {
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

    gcd_queue *prev_cur = gcd_current_queue;
    if (q && q->width == 1) gcd_current_queue = q;
    t->failed = gcd_invoke(t->op_array, &t->argv, t->argc);
    if (t->failed) {
        gcd_error_report(t);   /* reads EG(exception), reports, clears */
    }
    gcd_current_queue = prev_cur;

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
        gcd_pool_task_done();
        gcd_task_destroy(t);
    }
}

gcd_task *gcd_task_build(zval *closure, zval *argv)
{
    zend_op_array *src_oa = gcd_get_closure_oa(closure);
    if (!src_oa) {
        zend_throw_exception(zend_ce_type_error, "task must be a user-function Closure", 0);
        return NULL;
    }
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
    if (t->queue) gcd_queue_release(t->queue);
    if (t->group) gcd_group_release(t->group);
    if (t->owns_oa) gcd_op_array_free_persistent(t->op_array);
    gcd_zval_free(&t->argv, 1);
    pthread_mutex_destroy(&t->mutex);
    pthread_cond_destroy(&t->cond);
    pefree(t, 1);
}