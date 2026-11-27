#include "atomic.h"
#include <Zend/zend_exceptions.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    atomic_long value;
} gcd_atomic;

typedef struct {
    gcd_atomic *a;
    zend_object std;
} gcd_atomic_object;

static zend_class_entry *gcd_atomic_ce;
static zend_object_handlers atomic_handlers;

/* process-global named registry (persistent; lives for process lifetime) */
static HashTable g_registry;
static pthread_mutex_t g_reg_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline gcd_atomic_object *atomic_fetch(zend_object *obj)
{
    return (gcd_atomic_object *) ((char *) obj - XtOffsetOf(gcd_atomic_object, std));
}

static zend_object *atomic_create_object(zend_class_entry *ce)
{
    gcd_atomic_object *o = ecalloc(1, sizeof(gcd_atomic_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &atomic_handlers;
    o->a = NULL;
    return &o->std;
}

static void atomic_free_object(zend_object *object)
{
    zend_object_std_dtor(object);
}

static void registry_register(const char *name, size_t len, gcd_atomic *a)
{
    pthread_mutex_lock(&g_reg_mutex);
    zend_hash_str_update_ptr(&g_registry, name, len, a);
    pthread_mutex_unlock(&g_reg_mutex);
}

static gcd_atomic *registry_lookup(const char *name, size_t len)
{
    pthread_mutex_lock(&g_reg_mutex);
    gcd_atomic *a = zend_hash_str_find_ptr(&g_registry, name, len);
    pthread_mutex_unlock(&g_reg_mutex);
    return a;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_atomic_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(Atomic, __construct)
{
    zend_long v = 0;
    char *name = NULL;
    size_t name_len = 0;
    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(v)
        Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();

    gcd_atomic_object *o = atomic_fetch(Z_OBJ_P(ZEND_THIS));
    gcd_atomic *a = malloc(sizeof(gcd_atomic));
    atomic_init(&a->value, (long) v);
    o->a = a;
    if (name) {
        registry_register(name, name_len, a);
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_atomic_open, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(Atomic, open)
{
    char *name;
    size_t len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(name, len)
    ZEND_PARSE_PARAMETERS_END();

    gcd_atomic *a = registry_lookup(name, len);
    if (!a) {
        zend_throw_exception(zend_ce_value_error, "unknown GCD\\Atomic name", 0);
        RETURN_THROWS();
    }
    object_init_ex(return_value, gcd_atomic_ce);
    atomic_fetch(Z_OBJ_P(return_value))->a = a;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_atomic_step, 0, 0, 0)
    ZEND_ARG_INFO(0, n)
ZEND_END_ARG_INFO()

PHP_METHOD(Atomic, inc)
{
    zend_long n = 1;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(n)
    ZEND_PARSE_PARAMETERS_END();
    gcd_atomic *a = atomic_fetch(Z_OBJ_P(ZEND_THIS))->a;
    RETURN_LONG((zend_long) atomic_fetch_add(&a->value, (long) n) + n);
}

PHP_METHOD(Atomic, dec)
{
    zend_long n = 1;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(n)
    ZEND_PARSE_PARAMETERS_END();
    gcd_atomic *a = atomic_fetch(Z_OBJ_P(ZEND_THIS))->a;
    RETURN_LONG((zend_long) atomic_fetch_add(&a->value, -(long) n) - n);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_atomic_get, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Atomic, get)
{
    ZEND_PARSE_PARAMETERS_NONE();
    gcd_atomic *a = atomic_fetch(Z_OBJ_P(ZEND_THIS))->a;
    RETURN_LONG((zend_long) atomic_load(&a->value));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_atomic_set, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_METHOD(Atomic, set)
{
    zend_long v;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(v)
    ZEND_PARSE_PARAMETERS_END();
    gcd_atomic *a = atomic_fetch(Z_OBJ_P(ZEND_THIS))->a;
    atomic_store(&a->value, (long) v);
}

static const zend_function_entry atomic_methods[] = {
    PHP_ME(Atomic, __construct, arginfo_atomic_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Atomic, open, arginfo_atomic_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Atomic, inc, arginfo_atomic_step, ZEND_ACC_PUBLIC)
    PHP_ME(Atomic, dec, arginfo_atomic_step, ZEND_ACC_PUBLIC)
    PHP_ME(Atomic, get, arginfo_atomic_get, ZEND_ACC_PUBLIC)
    PHP_ME(Atomic, set, arginfo_atomic_set, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void gcd_atomic_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Atomic", atomic_methods);
    gcd_atomic_ce = zend_register_internal_class(&ce);
    gcd_atomic_ce->create_object = atomic_create_object;

    memcpy(&atomic_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    atomic_handlers.offset = XtOffsetOf(gcd_atomic_object, std);
    atomic_handlers.free_obj = atomic_free_object;

    zend_hash_init(&g_registry, 8, NULL, NULL, 1);
}