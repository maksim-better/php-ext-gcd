#include "group.h"
#include "queue.h"
#include "../runtime/task.h"
#include "../runtime/thread_pool.h"
#include "../runtime/copy.h"
#include <Zend/zend_exceptions.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

struct gcd_notify {
    gcd_queue *q;
    zend_op_array *handler;
    struct gcd_notify *next;
};

struct _gcd_group {
    atomic_long count;
    atomic_long refcount;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct gcd_notify *notify_head;
};

typedef struct {
    gcd_group *g;
    zend_object std;
} gcd_group_object;

static zend_class_entry *gcd_group_ce;
static zend_object_handlers group_handlers;

static inline gcd_group_object *group_fetch(zend_object *obj)
{
    return (gcd_group_object *) ((char *) obj - XtOffsetOf(gcd_group_object, std));
}

static zend_object *group_create_object(zend_class_entry *ce)
{
    gcd_group_object *o = ecalloc(1, sizeof(gcd_group_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &group_handlers;
    o->g = gcd_group_new();
    return &o->std;
}

static void group_free_object(zend_object *object)
{
    gcd_group_object *o = group_fetch(object);
    if (o->g) {
        gcd_group_release(o->g);
    }
    zend_object_std_dtor(object);
}

gcd_group *gcd_group_from_zval(zval *zv)
{
    if (Z_TYPE_P(zv) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(zv), gcd_group_ce)) {
        return NULL;
    }
    return group_fetch(Z_OBJ_P(zv))->g;
}

void gcd_group_leave(gcd_group *g)
{
    long old = atomic_fetch_sub(&g->count, 1);
    if (old == 1) {
        pthread_mutex_lock(&g->lock);
        /* Consume the notify list: each registration fires once (true GCD). */
        struct gcd_notify *head = g->notify_head;
        g->notify_head = NULL;
        for (struct gcd_notify *n = head; n; ) {
            struct gcd_notify *nx = n->next;
            if (n->handler && n->q) {
                gcd_task *t = gcd_task_create(n->handler, NULL, 0);
                t->owns_oa = 1;        /* task takes ownership of the handler */
                t->queue = n->q;
                gcd_queue_retain(n->q);
                t->is_sync = 0;
                t->no_leave = 1;       /* must not decrement the group count */
                t->group = g;          /* lifetime ref only (released on destroy) */
                gcd_group_retain(g);
                gcd_queue_submit(n->q, t);
            } else if (n->handler) {
                gcd_op_array_free_persistent(n->handler);
            }
            free(n);
            n = nx;
        }
        pthread_cond_broadcast(&g->cond);
        pthread_mutex_unlock(&g->lock);
    }
}

gcd_group *gcd_group_new(void)
{
    gcd_group *g = malloc(sizeof(gcd_group));
    atomic_init(&g->count, 0);
    atomic_init(&g->refcount, 1);
    pthread_mutex_init(&g->lock, NULL);
    pthread_cond_init(&g->cond, NULL);
    g->notify_head = NULL;
    return g;
}

void gcd_group_free(gcd_group *g)
{
    struct gcd_notify *n = g->notify_head;
    while (n) {
        struct gcd_notify *nx = n->next;
        if (n->handler) gcd_op_array_free_persistent(n->handler);
        free(n);
        n = nx;
    }
    pthread_mutex_destroy(&g->lock);
    pthread_cond_destroy(&g->cond);
    free(g);
}

void gcd_group_retain(gcd_group *g)
{
    if (g) atomic_fetch_add(&g->refcount, 1);
}

void gcd_group_release(gcd_group *g)
{
    if (g && atomic_fetch_sub(&g->refcount, 1) == 1) {
        gcd_group_free(g);
    }
}

void gcd_group_enter(gcd_group *g)
{
    atomic_fetch_add(&g->count, 1);
}

void gcd_group_wait(gcd_group *g)
{
    pthread_mutex_lock(&g->lock);
    while (atomic_load(&g->count) > 0) {
        pthread_cond_wait(&g->cond, &g->lock);
    }
    pthread_mutex_unlock(&g->lock);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_group_create, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(group_create)
{
    ZEND_PARSE_PARAMETERS_NONE();
    object_init_ex(return_value, gcd_group_ce);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_group_one, 0, 0, 1)
    ZEND_ARG_INFO(0, group)
ZEND_END_ARG_INFO()

PHP_FUNCTION(group_enter)
{
    zval *gobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(gobj) ZEND_PARSE_PARAMETERS_END();
    gcd_group *g = gcd_group_from_zval(gobj);
    if (!g) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Group", 0); RETURN_THROWS(); }
    atomic_fetch_add(&g->count, 1);
}

PHP_FUNCTION(group_leave)
{
    zval *gobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(gobj) ZEND_PARSE_PARAMETERS_END();
    gcd_group *g = gcd_group_from_zval(gobj);
    if (!g) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Group", 0); RETURN_THROWS(); }
    gcd_group_leave(g);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_group_wait, 0, 0, 1)
    ZEND_ARG_INFO(0, group)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

PHP_FUNCTION(group_wait)
{
    zval *gobj;
    double timeout = -1;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJECT(gobj)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END();
    gcd_group *g = gcd_group_from_zval(gobj);
    if (!g) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Group", 0); RETURN_THROWS(); }

    pthread_mutex_lock(&g->lock);
    while (atomic_load(&g->count) > 0) {
        if (timeout == 0) {
            pthread_mutex_unlock(&g->lock);
            RETURN_BOOL(0);
        } else if (timeout < 0) {
            pthread_cond_wait(&g->cond, &g->lock);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += (time_t) timeout;
            ts.tv_nsec += (long) ((timeout - (time_t) timeout) * 1e9);
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
            int rc = pthread_cond_timedwait(&g->cond, &g->lock, &ts);
            if (rc == ETIMEDOUT && atomic_load(&g->count) > 0) {
                pthread_mutex_unlock(&g->lock);
                RETURN_BOOL(0);
            }
        }
    }
    pthread_mutex_unlock(&g->lock);
    RETURN_BOOL(1);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_group_async, 0, 0, 3)
    ZEND_ARG_INFO(0, group)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, task)
    ZEND_ARG_INFO(0, argv)
ZEND_END_ARG_INFO()

PHP_FUNCTION(group_async)
{
    zval *gobj, *qobj, *closure, *argv = NULL;
    ZEND_PARSE_PARAMETERS_START(3, 4)
        Z_PARAM_OBJECT(gobj)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(closure)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(argv)
    ZEND_PARSE_PARAMETERS_END();
    gcd_group *g = gcd_group_from_zval(gobj);
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!g || !q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Group and GCD\\Queue", 0); RETURN_THROWS(); }

    gcd_task *t = gcd_task_build(closure, argv);
    if (!t) RETURN_THROWS();
    atomic_fetch_add(&g->count, 1);
    t->queue = q;
    gcd_queue_retain(q);
    t->is_sync = 0;
    t->group = g;
    gcd_group_retain(g);
    gcd_queue_submit(q, t);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_group_notify, 0, 0, 3)
    ZEND_ARG_INFO(0, group)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, task)
ZEND_END_ARG_INFO()

PHP_FUNCTION(group_notify)
{
    zval *gobj, *qobj, *closure;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_OBJECT(gobj)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(closure)
    ZEND_PARSE_PARAMETERS_END();
    gcd_group *g = gcd_group_from_zval(gobj);
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!g || !q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Group and GCD\\Queue", 0); RETURN_THROWS(); }
    zend_op_array *src = gcd_get_closure_oa(closure);
    if (!src) { zend_throw_exception(zend_ce_type_error, "notify must be a user-function Closure", 0); RETURN_THROWS(); }
    zend_op_array *copy = gcd_op_array_copy_persistent(src);
    if (!copy) { zend_throw_exception(gcd_capture_ce, "notify closure unsupported in v1", 0); RETURN_THROWS(); }
    struct gcd_notify *n = malloc(sizeof(struct gcd_notify));
    n->q = q;
    n->handler = copy;
    pthread_mutex_lock(&g->lock);
    n->next = g->notify_head;
    g->notify_head = n;
    pthread_mutex_unlock(&g->lock);
}

void gcd_group_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Group", NULL);
    gcd_group_ce = zend_register_internal_class(&ce);
    gcd_group_ce->create_object = group_create_object;
    memcpy(&group_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    group_handlers.offset = XtOffsetOf(gcd_group_object, std);
    group_handlers.free_obj = group_free_object;
}