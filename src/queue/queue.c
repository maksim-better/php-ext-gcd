#include "queue.h"
#include "../runtime/task.h"
#include "../runtime/thread_pool.h"
#include "../runtime/copy.h"
#include <Zend/zend_exceptions.h>
#include <stdlib.h>
#include <string.h>

zend_class_entry *gcd_queue_ce;
zend_class_entry *gcd_capture_ce;
zend_class_entry *gcd_serialization_ce;
zend_class_entry *gcd_deadlock_ce;
static zend_object_handlers gcd_queue_handlers;

static void specific_dtor(zval *zv) { gcd_zval_free(zv, 1); }

static inline gcd_queue_object *gcd_queue_fetch(zend_object *obj)
{
    return (gcd_queue_object *) ((char *) obj - XtOffsetOf(gcd_queue_object, std));
}

static zend_object *gcd_queue_create_object(zend_class_entry *ce)
{
    gcd_queue_object *obj = ecalloc(1, sizeof(gcd_queue_object) + zend_object_properties_size(ce));
    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &gcd_queue_handlers;
    obj->owns = 1;
    obj->q = calloc(1, sizeof(gcd_queue));
    obj->q->width = 1;
    atomic_init(&obj->q->refcount, 1);
    pthread_mutex_init(&obj->q->drain_lock, NULL);
    pthread_mutex_init(&obj->q->list_mutex, NULL);
    pthread_mutex_init(&obj->q->width_mutex, NULL);
    pthread_cond_init(&obj->q->width_cond, NULL);
    zend_hash_init(&obj->q->specific, 4, NULL, specific_dtor, 1);
    obj->q->qos = -1;
    obj->q->target = NULL;
    return &obj->std;
}

static void gcd_queue_free_object(zend_object *object)
{
    gcd_queue_object *obj = gcd_queue_fetch(object);
    if (obj->q && obj->owns) {
        gcd_queue_release(obj->q);
    }
    zend_object_std_dtor(object);
}

void gcd_queue_retain(gcd_queue *q)
{
    if (q) atomic_fetch_add(&q->refcount, 1);
}

void gcd_queue_release(gcd_queue *q)
{
    if (!q) return;
    if (atomic_fetch_sub(&q->refcount, 1) == 1) {
        if (q->target) gcd_queue_release(q->target);
        pthread_mutex_destroy(&q->drain_lock);
        pthread_mutex_destroy(&q->list_mutex);
        pthread_mutex_destroy(&q->width_mutex);
        pthread_cond_destroy(&q->width_cond);
        zend_hash_destroy(&q->specific);
        free(q->label);
        free(q);
    }
}

gcd_queue *gcd_queue_from_zval(zval *zv)
{
    if (Z_TYPE_P(zv) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(zv), gcd_queue_ce)) {
        return NULL;
    }
    return gcd_queue_fetch(Z_OBJ_P(zv))->q;
}

void gcd_queue_wrap_borrowed(zval *return_value, gcd_queue *q)
{
    object_init_ex(return_value, gcd_queue_ce);
    gcd_queue_object *obj = gcd_queue_fetch(Z_OBJ_P(return_value));
    if (obj->q && obj->owns) {
        gcd_queue_release(obj->q);   /* drop auto-created queue */
    }
    obj->q = q;
    obj->owns = 0;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_current_queue, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(current_queue)
{
    ZEND_PARSE_PARAMETERS_NONE();
    extern __thread gcd_queue *gcd_current_queue;
    if (!gcd_current_queue) RETURN_NULL();
    gcd_queue_wrap_borrowed(return_value, gcd_current_queue);
}

static gcd_queue *g_main_queue = NULL;

ZEND_BEGIN_ARG_INFO_EX(arginfo_main_queue, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(main_queue)
{
    ZEND_PARSE_PARAMETERS_NONE();
    if (!g_main_queue) {
        g_main_queue = calloc(1, sizeof(gcd_queue));
        g_main_queue->width = 1;
        atomic_init(&g_main_queue->refcount, 1);
        g_main_queue->label = strdup("main");
        g_main_queue->qos = -1;
        pthread_mutex_init(&g_main_queue->drain_lock, NULL);
        pthread_mutex_init(&g_main_queue->list_mutex, NULL);
        pthread_mutex_init(&g_main_queue->width_mutex, NULL);
        pthread_cond_init(&g_main_queue->width_cond, NULL);
        zend_hash_init(&g_main_queue->specific, 4, NULL, specific_dtor, 1);
    }
    gcd_queue_wrap_borrowed(return_value, g_main_queue);
}

#define GCD_QOS_DEFAULT 2

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_set_qos, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, qos)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_set_qos)
{
    zval *qobj;
    zend_long qos;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_LONG(qos)
    ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    q->qos = (int) qos;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_get_qos, 0, 0, 1)
    ZEND_ARG_INFO(0, queue)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_get_qos)
{
    zval *qobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(qobj) ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    int qos = GCD_QOS_DEFAULT;
    for (gcd_queue *cur = q; cur; cur = cur->target) {
        if (cur->qos >= 0) { qos = cur->qos; break; }
    }
    RETURN_LONG(qos);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_set_target, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, target)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_set_target)
{
    zval *qobj, *tobj;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(tobj)
    ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    gcd_queue *t = gcd_queue_from_zval(tobj);
    if (!q || !t) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    if (q == t) { zend_throw_exception(zend_ce_value_error, "queue cannot target itself", 0); RETURN_THROWS(); }
    for (gcd_queue *cur = t; cur; cur = cur->target) {
        if (cur == q) { zend_throw_exception(zend_ce_value_error, "target queue cycle", 0); RETURN_THROWS(); }
    }
    gcd_queue_retain(t);
    if (q->target) gcd_queue_release(q->target);
    q->target = t;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_qos_class_self, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(qos_class_self)
{
    ZEND_PARSE_PARAMETERS_NONE();
    /* Linux best-effort: v1 reports the default class */
    RETURN_LONG(GCD_QOS_DEFAULT);
}

void gcd_queue_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Queue", NULL);
    gcd_queue_ce = zend_register_internal_class(&ce);
    gcd_queue_ce->create_object = gcd_queue_create_object;

    memcpy(&gcd_queue_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    gcd_queue_handlers.offset = XtOffsetOf(gcd_queue_object, std);
    gcd_queue_handlers.free_obj = gcd_queue_free_object;

    INIT_CLASS_ENTRY(ce, "GCD\\CaptureException", NULL);
    gcd_capture_ce = zend_register_internal_class_ex(&ce, zend_ce_error);

    INIT_CLASS_ENTRY(ce, "GCD\\SerializationException", NULL);
    gcd_serialization_ce = zend_register_internal_class_ex(&ce, zend_ce_error);

    INIT_CLASS_ENTRY(ce, "GCD\\DeadlockException", NULL);
    gcd_deadlock_ce = zend_register_internal_class_ex(&ce, zend_ce_error);

    zend_register_long_constant("GCD\\QOS_USER_INTERACTIVE", sizeof("GCD\\QOS_USER_INTERACTIVE") - 1, 4, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\QOS_USER_INITIATED", sizeof("GCD\\QOS_USER_INITIATED") - 1, 3, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\QOS_DEFAULT", sizeof("GCD\\QOS_DEFAULT") - 1, 2, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\QOS_UTILITY", sizeof("GCD\\QOS_UTILITY") - 1, 1, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\QOS_BACKGROUND", sizeof("GCD\\QOS_BACKGROUND") - 1, 0, CONST_CS | CONST_PERSISTENT, 0);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_create, 0, 0, 1)
    ZEND_ARG_INFO(0, label)
    ZEND_ARG_INFO(0, width)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_create)
{
    char *label;
    size_t label_len;
    zend_long width = 1;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(label, label_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(width)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, gcd_queue_ce);
    gcd_queue *q = gcd_queue_from_zval(return_value);
    q->label = strndup(label, label_len);
    q->width = width < 1 ? 1 : (int) width;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_global_queue, 0, 0, 0)
    ZEND_ARG_INFO(0, qos)
    ZEND_ARG_INFO(0, pool_size)
ZEND_END_ARG_INFO()

PHP_FUNCTION(global_queue)
{
    zend_long qos = 0, pool_size = 0;
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(qos)
        Z_PARAM_LONG(pool_size)
    ZEND_PARSE_PARAMETERS_END();

    object_init_ex(return_value, gcd_queue_ce);
    gcd_queue *q = gcd_queue_from_zval(return_value);
    q->label = strdup("global");
    q->width = 64;
}

static void submit_drain_job(gcd_queue *q)
{
    gcd_task *job = pemalloc(sizeof(gcd_task), 1);
    memset(job, 0, sizeof(*job));
    job->op_array = NULL;   /* marks a drain job, not a closure task */
    job->queue = q;
    gcd_queue_retain(q);    /* drain job holds a reference */
    gcd_pool_submit(job);
}

void gcd_queue_enqueue(gcd_queue *q, gcd_task *t)
{
    gcd_pool_pending_inc();
    pthread_mutex_lock(&q->list_mutex);
    t->next = NULL;
    if (q->tail) q->tail->next = t; else q->head = t;
    q->tail = t;
    int need = !q->draining;
    if (need) q->draining = 1;
    pthread_mutex_unlock(&q->list_mutex);
    if (need) submit_drain_job(q);
}

void gcd_queue_drain(gcd_queue *q)
{
    pthread_mutex_lock(&q->drain_lock);
    pthread_mutex_lock(&q->width_mutex);
    while (q->suspended) {
        pthread_cond_wait(&q->width_cond, &q->width_mutex);
    }
    pthread_mutex_unlock(&q->width_mutex);
    for (;;) {
        pthread_mutex_lock(&q->list_mutex);
        gcd_task *t = q->head;
        if (t) { q->head = t->next; if (!q->head) q->tail = NULL; }
        pthread_mutex_unlock(&q->list_mutex);
        if (!t) break;
        pthread_mutex_lock(&q->width_mutex);
        while (q->suspended) {
            pthread_cond_wait(&q->width_cond, &q->width_mutex);
        }
        pthread_mutex_unlock(&q->width_mutex);
        gcd_task_execute(t);   /* FIFO order under drain_lock */
    }
    pthread_mutex_unlock(&q->drain_lock);

    pthread_mutex_lock(&q->list_mutex);
    q->draining = 0;
    int again = q->head != NULL;
    if (again) q->draining = 1;
    pthread_mutex_unlock(&q->list_mutex);
    if (again) submit_drain_job(q);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_one, 0, 0, 1)
    ZEND_ARG_INFO(0, queue)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_suspend)
{
    zval *qobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(qobj) ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    pthread_mutex_lock(&q->width_mutex);
    q->suspended = 1;
    pthread_mutex_unlock(&q->width_mutex);
}

PHP_FUNCTION(queue_resume)
{
    zval *qobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(qobj) ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    pthread_mutex_lock(&q->width_mutex);
    q->suspended = 0;
    pthread_cond_broadcast(&q->width_cond);
    pthread_mutex_unlock(&q->width_mutex);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_set_specific, 0, 0, 3)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_set_specific)
{
    zval *qobj, *val;
    char *key; size_t len;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_STRING(key, len)
        Z_PARAM_ZVAL(val)
    ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    zval copy;
    if (gcd_zval_copy_persistent(&copy, val) == FAILURE) {
        zend_throw_exception(gcd_serialization_ce, "value is not serializable", 0);
        RETURN_THROWS();
    }
    pthread_mutex_lock(&q->width_mutex);
    zend_hash_str_update(&q->specific, key, len, &copy);
    pthread_mutex_unlock(&q->width_mutex);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_get_specific, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_get_specific)
{
    zval *qobj;
    char *key; size_t len;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_STRING(key, len)
    ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    pthread_mutex_lock(&q->width_mutex);
    zval *found = zend_hash_str_find(&q->specific, key, len);
    if (found) {
        zval ret;
        gcd_zval_copy(&ret, found, 0);
        pthread_mutex_unlock(&q->width_mutex);
        RETURN_COPY_VALUE(&ret);
    }
    pthread_mutex_unlock(&q->width_mutex);
    RETURN_NULL();
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_queue_set_error_handler, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, handler)
ZEND_END_ARG_INFO()

PHP_FUNCTION(queue_set_error_handler)
{
    zval *qobj, *closure;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(closure)
    ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    zend_op_array *src = gcd_get_closure_oa(closure);
    if (!src) { zend_throw_exception(zend_ce_type_error, "handler must be a user-function Closure", 0); RETURN_THROWS(); }
    zend_op_array *copy = gcd_op_array_copy_persistent(src);
    if (!copy) {
        zend_throw_exception(gcd_capture_ce, "handler closure unsupported in v1", 0);
        RETURN_THROWS();
    }
    if (q->error_handler) gcd_op_array_free_persistent(q->error_handler);
    q->error_handler = copy;
}