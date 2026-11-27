#include "semaphore.h"
#include <Zend/zend_exceptions.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    long value;
} gcd_semaphore;

typedef struct {
    gcd_semaphore *s;
    zend_object std;
} gcd_semaphore_object;

static zend_class_entry *gcd_semaphore_ce;
static zend_object_handlers semaphore_handlers;
static HashTable g_registry;
static pthread_mutex_t g_reg_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline gcd_semaphore_object *sem_fetch(zend_object *obj)
{
    return (gcd_semaphore_object *) ((char *) obj - XtOffsetOf(gcd_semaphore_object, std));
}

static zend_object *sem_create_object(zend_class_entry *ce)
{
    gcd_semaphore_object *o = ecalloc(1, sizeof(gcd_semaphore_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &semaphore_handlers;
    return &o->std;
}

static void sem_free_object(zend_object *object) { zend_object_std_dtor(object); }

static gcd_semaphore *sem_from_zval(zval *zv)
{
    if (Z_TYPE_P(zv) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(zv), gcd_semaphore_ce)) {
        return NULL;
    }
    return sem_fetch(Z_OBJ_P(zv))->s;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_sem_create, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_FUNCTION(semaphore_create)
{
    zend_long value;
    char *name = NULL; size_t name_len = 0;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_LONG(value)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();
    object_init_ex(return_value, gcd_semaphore_ce);
    gcd_semaphore *s = malloc(sizeof(gcd_semaphore));
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->value = (long) value;
    sem_fetch(Z_OBJ_P(return_value))->s = s;
    if (name) {
        pthread_mutex_lock(&g_reg_mutex);
        zend_hash_str_update_ptr(&g_registry, name, name_len, s);
        pthread_mutex_unlock(&g_reg_mutex);
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_sem_open, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_FUNCTION(semaphore_open)
{
    char *name; size_t len;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_STRING(name, len) ZEND_PARSE_PARAMETERS_END();
    pthread_mutex_lock(&g_reg_mutex);
    gcd_semaphore *s = zend_hash_str_find_ptr(&g_registry, name, len);
    pthread_mutex_unlock(&g_reg_mutex);
    if (!s) { zend_throw_exception(zend_ce_value_error, "unknown GCD\\Semaphore name", 0); RETURN_THROWS(); }
    object_init_ex(return_value, gcd_semaphore_ce);
    sem_fetch(Z_OBJ_P(return_value))->s = s;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_sem_wait, 0, 0, 1)
    ZEND_ARG_INFO(0, semaphore)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

PHP_FUNCTION(semaphore_wait)
{
    zval *sobj;
    double timeout = -1;
    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_OBJECT(sobj)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END();
    gcd_semaphore *s = sem_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Semaphore", 0); RETURN_THROWS(); }

    pthread_mutex_lock(&s->lock);
    while (s->value <= 0) {
        if (timeout == 0) {
            pthread_mutex_unlock(&s->lock);
            RETURN_BOOL(0);
        } else if (timeout < 0) {
            pthread_cond_wait(&s->cond, &s->lock);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += (time_t) timeout;
            ts.tv_nsec += (long) ((timeout - (time_t) timeout) * 1e9);
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
            int rc = pthread_cond_timedwait(&s->cond, &s->lock, &ts);
            if (rc == ETIMEDOUT && s->value <= 0) {
                pthread_mutex_unlock(&s->lock);
                RETURN_BOOL(0);
            }
        }
    }
    s->value--;
    pthread_mutex_unlock(&s->lock);
    RETURN_BOOL(1);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_sem_signal, 0, 0, 1)
    ZEND_ARG_INFO(0, semaphore)
ZEND_END_ARG_INFO()

PHP_FUNCTION(semaphore_signal)
{
    zval *sobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(sobj) ZEND_PARSE_PARAMETERS_END();
    gcd_semaphore *s = sem_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Semaphore", 0); RETURN_THROWS(); }
    pthread_mutex_lock(&s->lock);
    s->value++;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->lock);
}

void gcd_semaphore_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Semaphore", NULL);
    gcd_semaphore_ce = zend_register_internal_class(&ce);
    gcd_semaphore_ce->create_object = sem_create_object;
    memcpy(&semaphore_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    semaphore_handlers.offset = XtOffsetOf(gcd_semaphore_object, std);
    semaphore_handlers.free_obj = sem_free_object;
    zend_hash_init(&g_registry, 8, NULL, NULL, 1);
}