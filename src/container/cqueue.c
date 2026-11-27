#include "cqueue.h"
#include "../runtime/copy.h"
#include "../queue/queue.h"
#include <Zend/zend_exceptions.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

typedef struct _gcd_cq_node {
    zval val;                    /* persistent */
    struct _gcd_cq_node *next;
} gcd_cq_node;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    gcd_cq_node *head, *tail;
    int count;
} gcd_cqueue;

typedef struct {
    gcd_cqueue *q;
    zend_object std;
} gcd_cqueue_object;

static zend_class_entry *gcd_cqueue_ce;
static zend_object_handlers cqueue_handlers;
static HashTable g_registry;
static pthread_mutex_t g_reg_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline gcd_cqueue_object *cqueue_fetch(zend_object *obj)
{
    return (gcd_cqueue_object *) ((char *) obj - XtOffsetOf(gcd_cqueue_object, std));
}

static zend_object *cqueue_create_object(zend_class_entry *ce)
{
    gcd_cqueue_object *o = ecalloc(1, sizeof(gcd_cqueue_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &cqueue_handlers;
    return &o->std;
}

static void cqueue_free_object(zend_object *object)
{
    zend_object_std_dtor(object);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_cqueue_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(ConcurrentQueue, __construct)
{
    char *name = NULL;
    size_t name_len = 0;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();

    gcd_cqueue *q = malloc(sizeof(gcd_cqueue));
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->head = q->tail = NULL;
    q->count = 0;
    cqueue_fetch(Z_OBJ_P(ZEND_THIS))->q = q;
    if (name) {
        pthread_mutex_lock(&g_reg_mutex);
        zend_hash_str_update_ptr(&g_registry, name, name_len, q);
        pthread_mutex_unlock(&g_reg_mutex);
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_cqueue_open, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(ConcurrentQueue, open)
{
    char *name;
    size_t len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(name, len)
    ZEND_PARSE_PARAMETERS_END();
    pthread_mutex_lock(&g_reg_mutex);
    gcd_cqueue *q = zend_hash_str_find_ptr(&g_registry, name, len);
    pthread_mutex_unlock(&g_reg_mutex);
    if (!q) {
        zend_throw_exception(zend_ce_value_error, "unknown GCD\\ConcurrentQueue name", 0);
        RETURN_THROWS();
    }
    object_init_ex(return_value, gcd_cqueue_ce);
    cqueue_fetch(Z_OBJ_P(return_value))->q = q;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_cqueue_push, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_METHOD(ConcurrentQueue, push)
{
    zval *v;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ZVAL(v)
    ZEND_PARSE_PARAMETERS_END();
    gcd_cqueue *q = cqueue_fetch(Z_OBJ_P(ZEND_THIS))->q;
    gcd_cq_node *n = pemalloc(sizeof(gcd_cq_node), 1);
    if (gcd_zval_copy_persistent(&n->val, v) == FAILURE) {
        pefree(n, 1);
        zend_throw_exception(gcd_serialization_ce, "value is not serializable", 0);
        RETURN_THROWS();
    }
    n->next = NULL;
    pthread_mutex_lock(&q->lock);
    if (q->tail) q->tail->next = n; else q->head = n;
    q->tail = n;
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_cqueue_pop, 0, 0, 0)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

PHP_METHOD(ConcurrentQueue, pop)
{
    double timeout = -1;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END();
    gcd_cqueue *q = cqueue_fetch(Z_OBJ_P(ZEND_THIS))->q;

    pthread_mutex_lock(&q->lock);
    while (!q->head) {
        if (timeout == 0) {
            pthread_mutex_unlock(&q->lock);
            RETURN_NULL();
        } else if (timeout < 0) {
            pthread_cond_wait(&q->cond, &q->lock);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += (time_t) timeout;
            ts.tv_nsec += (long) ((timeout - (time_t) timeout) * 1e9);
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
            int rc = pthread_cond_timedwait(&q->cond, &q->lock, &ts);
            if (rc == ETIMEDOUT && !q->head) {
                pthread_mutex_unlock(&q->lock);
                RETURN_NULL();
            }
        }
    }
    gcd_cq_node *n = q->head;
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    q->count--;
    pthread_mutex_unlock(&q->lock);

    zval ret;
    gcd_zval_copy(&ret, &n->val, 0);
    gcd_zval_free(&n->val, 1);
    pefree(n, 1);
    RETURN_COPY_VALUE(&ret);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_cqueue_trypop, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(ConcurrentQueue, tryPop)
{
    ZEND_PARSE_PARAMETERS_NONE();
    gcd_cqueue *q = cqueue_fetch(Z_OBJ_P(ZEND_THIS))->q;
    pthread_mutex_lock(&q->lock);
    gcd_cq_node *n = q->head;
    if (n) {
        q->head = n->next;
        if (!q->head) q->tail = NULL;
        q->count--;
    }
    pthread_mutex_unlock(&q->lock);
    if (!n) RETURN_NULL();
    zval ret;
    gcd_zval_copy(&ret, &n->val, 0);
    gcd_zval_free(&n->val, 1);
    pefree(n, 1);
    RETURN_COPY_VALUE(&ret);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_cqueue_count, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(ConcurrentQueue, count)
{
    ZEND_PARSE_PARAMETERS_NONE();
    gcd_cqueue *q = cqueue_fetch(Z_OBJ_P(ZEND_THIS))->q;
    pthread_mutex_lock(&q->lock);
    zend_long c = q->count;
    pthread_mutex_unlock(&q->lock);
    RETURN_LONG(c);
}

static const zend_function_entry cqueue_methods[] = {
    PHP_ME(ConcurrentQueue, __construct, arginfo_cqueue_construct, ZEND_ACC_PUBLIC)
    PHP_ME(ConcurrentQueue, open, arginfo_cqueue_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(ConcurrentQueue, push, arginfo_cqueue_push, ZEND_ACC_PUBLIC)
    PHP_ME(ConcurrentQueue, pop, arginfo_cqueue_pop, ZEND_ACC_PUBLIC)
    PHP_ME(ConcurrentQueue, tryPop, arginfo_cqueue_trypop, ZEND_ACC_PUBLIC)
    PHP_ME(ConcurrentQueue, count, arginfo_cqueue_count, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void gcd_cqueue_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\ConcurrentQueue", cqueue_methods);
    gcd_cqueue_ce = zend_register_internal_class(&ce);
    gcd_cqueue_ce->create_object = cqueue_create_object;
    memcpy(&cqueue_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    cqueue_handlers.offset = XtOffsetOf(gcd_cqueue_object, std);
    cqueue_handlers.free_obj = cqueue_free_object;
    zend_hash_init(&g_registry, 8, NULL, NULL, 1);
}