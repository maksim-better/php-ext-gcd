#include "map.h"
#include "../runtime/copy.h"
#include "../queue/queue.h"
#include <Zend/zend_exceptions.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    pthread_mutex_t lock;
    HashTable ht;              /* persistent; string keys -> persistent zvals */
} gcd_map;

typedef struct {
    gcd_map *m;
    zend_object std;
} gcd_map_object;

static zend_class_entry *gcd_map_ce;
static zend_object_handlers map_handlers;
static HashTable g_registry;
static pthread_mutex_t g_reg_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline gcd_map_object *map_fetch(zend_object *obj)
{
    return (gcd_map_object *) ((char *) obj - XtOffsetOf(gcd_map_object, std));
}

static zend_object *map_create_object(zend_class_entry *ce)
{
    gcd_map_object *o = ecalloc(1, sizeof(gcd_map_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &map_handlers;
    return &o->std;
}

static void map_free_object(zend_object *object) { zend_object_std_dtor(object); }

static void map_val_dtor(zval *zv) { gcd_zval_free(zv, 1); }

ZEND_BEGIN_ARG_INFO_EX(arginfo_map_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(Map, __construct)
{
    char *name = NULL; size_t name_len = 0;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();
    gcd_map *m = malloc(sizeof(gcd_map));
    pthread_mutex_init(&m->lock, NULL);
    zend_hash_init(&m->ht, 8, NULL, map_val_dtor, 1);
    map_fetch(Z_OBJ_P(ZEND_THIS))->m = m;
    if (name) {
        pthread_mutex_lock(&g_reg_mutex);
        zend_hash_str_update_ptr(&g_registry, name, name_len, m);
        pthread_mutex_unlock(&g_reg_mutex);
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_map_open, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(Map, open)
{
    char *name; size_t len;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_STRING(name, len) ZEND_PARSE_PARAMETERS_END();
    pthread_mutex_lock(&g_reg_mutex);
    gcd_map *m = zend_hash_str_find_ptr(&g_registry, name, len);
    pthread_mutex_unlock(&g_reg_mutex);
    if (!m) { zend_throw_exception(zend_ce_value_error, "unknown GCD\\Map name", 0); RETURN_THROWS(); }
    object_init_ex(return_value, gcd_map_ce);
    map_fetch(Z_OBJ_P(return_value))->m = m;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_map_set, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_METHOD(Map, set)
{
    char *k; size_t klen; zval *v;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(k, klen)
        Z_PARAM_ZVAL(v)
    ZEND_PARSE_PARAMETERS_END();
    gcd_map *m = map_fetch(Z_OBJ_P(ZEND_THIS))->m;
    zval copy;
    if (gcd_zval_copy_persistent(&copy, v) == FAILURE) {
        zend_throw_exception(gcd_serialization_ce, "value is not serializable", 0);
        RETURN_THROWS();
    }
    pthread_mutex_lock(&m->lock);
    zend_hash_str_update(&m->ht, k, klen, &copy);
    pthread_mutex_unlock(&m->lock);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_map_key, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

PHP_METHOD(Map, get)
{
    char *k; size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_STRING(k, klen) ZEND_PARSE_PARAMETERS_END();
    gcd_map *m = map_fetch(Z_OBJ_P(ZEND_THIS))->m;
    pthread_mutex_lock(&m->lock);
    zval *found = zend_hash_str_find(&m->ht, k, klen);
    if (found) {
        zval ret;
        gcd_zval_copy(&ret, found, 0);
        pthread_mutex_unlock(&m->lock);
        RETURN_COPY_VALUE(&ret);
    }
    pthread_mutex_unlock(&m->lock);
    RETURN_NULL();
}

PHP_METHOD(Map, has)
{
    char *k; size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_STRING(k, klen) ZEND_PARSE_PARAMETERS_END();
    gcd_map *m = map_fetch(Z_OBJ_P(ZEND_THIS))->m;
    pthread_mutex_lock(&m->lock);
    int exists = zend_hash_str_exists(&m->ht, k, klen);
    pthread_mutex_unlock(&m->lock);
    RETURN_BOOL(exists);
}

PHP_METHOD(Map, delete)
{
    char *k; size_t klen;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_STRING(k, klen) ZEND_PARSE_PARAMETERS_END();
    gcd_map *m = map_fetch(Z_OBJ_P(ZEND_THIS))->m;
    pthread_mutex_lock(&m->lock);
    int removed = zend_hash_str_del(&m->ht, k, klen) == SUCCESS;
    pthread_mutex_unlock(&m->lock);
    RETURN_BOOL(removed);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_map_count, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Map, count)
{
    ZEND_PARSE_PARAMETERS_NONE();
    gcd_map *m = map_fetch(Z_OBJ_P(ZEND_THIS))->m;
    pthread_mutex_lock(&m->lock);
    zend_long c = zend_hash_num_elements(&m->ht);
    pthread_mutex_unlock(&m->lock);
    RETURN_LONG(c);
}

static const zend_function_entry map_methods[] = {
    PHP_ME(Map, __construct, arginfo_map_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Map, open, arginfo_map_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Map, set, arginfo_map_set, ZEND_ACC_PUBLIC)
    PHP_ME(Map, get, arginfo_map_key, ZEND_ACC_PUBLIC)
    PHP_ME(Map, has, arginfo_map_key, ZEND_ACC_PUBLIC)
    PHP_ME(Map, delete, arginfo_map_key, ZEND_ACC_PUBLIC)
    PHP_ME(Map, count, arginfo_map_count, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void gcd_map_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Map", map_methods);
    gcd_map_ce = zend_register_internal_class(&ce);
    gcd_map_ce->create_object = map_create_object;
    memcpy(&map_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    map_handlers.offset = XtOffsetOf(gcd_map_object, std);
    map_handlers.free_obj = map_free_object;
    zend_hash_init(&g_registry, 8, NULL, NULL, 1);
}