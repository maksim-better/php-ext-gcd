#include "set.h"
#include "../queue/queue.h"
#include <Zend/zend_exceptions.h>
#include <ext/standard/php_var.h>
#include <Zend/zend_smart_str.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    pthread_mutex_t lock;
    HashTable ht;              /* persistent; serialized-value keys */
} gcd_set;

typedef struct {
    gcd_set *s;
    zend_object std;
} gcd_set_object;

static zend_class_entry *gcd_set_ce;
static zend_object_handlers set_handlers;
static HashTable g_registry;
static pthread_mutex_t g_reg_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline gcd_set_object *set_fetch(zend_object *obj)
{
    return (gcd_set_object *) ((char *) obj - XtOffsetOf(gcd_set_object, std));
}

static zend_object *set_create_object(zend_class_entry *ce)
{
    gcd_set_object *o = ecalloc(1, sizeof(gcd_set_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &set_handlers;
    return &o->std;
}

static void set_free_object(zend_object *object) { zend_object_std_dtor(object); }

/* serialize a value into a request-local key string */
static void make_key(zval *v, char **key, size_t *len)
{
    smart_str buf = {0};
    php_serialize_data_t sd = php_var_serialize_init();
    php_var_serialize(&buf, v, &sd);
    php_var_serialize_destroy(sd);
    *len = ZSTR_LEN(buf.s);
    *key = estrndup(ZSTR_VAL(buf.s), *len);
    zend_string_release(buf.s);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_construct, 0, 0, 0)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(Set, __construct)
{
    char *name = NULL; size_t name_len = 0;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();
    gcd_set *s = malloc(sizeof(gcd_set));
    pthread_mutex_init(&s->lock, NULL);
    zend_hash_init(&s->ht, 8, NULL, NULL, 1);
    set_fetch(Z_OBJ_P(ZEND_THIS))->s = s;
    if (name) {
        pthread_mutex_lock(&g_reg_mutex);
        zend_hash_str_update_ptr(&g_registry, name, name_len, s);
        pthread_mutex_unlock(&g_reg_mutex);
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_open, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

PHP_METHOD(Set, open)
{
    char *name; size_t len;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_STRING(name, len) ZEND_PARSE_PARAMETERS_END();
    pthread_mutex_lock(&g_reg_mutex);
    gcd_set *s = zend_hash_str_find_ptr(&g_registry, name, len);
    pthread_mutex_unlock(&g_reg_mutex);
    if (!s) { zend_throw_exception(zend_ce_value_error, "unknown GCD\\Set name", 0); RETURN_THROWS(); }
    object_init_ex(return_value, gcd_set_ce);
    set_fetch(Z_OBJ_P(return_value))->s = s;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_value, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

PHP_METHOD(Set, add)
{
    zval *v;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_ZVAL(v) ZEND_PARSE_PARAMETERS_END();
    gcd_set *s = set_fetch(Z_OBJ_P(ZEND_THIS))->s;
    char *key; size_t len;
    make_key(v, &key, &len);
    pthread_mutex_lock(&s->lock);
    int added = !zend_hash_str_exists(&s->ht, key, len);
    if (added) zend_hash_str_add_empty_element(&s->ht, key, len);
    pthread_mutex_unlock(&s->lock);
    efree(key);
    RETURN_BOOL(added);
}

PHP_METHOD(Set, contains)
{
    zval *v;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_ZVAL(v) ZEND_PARSE_PARAMETERS_END();
    gcd_set *s = set_fetch(Z_OBJ_P(ZEND_THIS))->s;
    char *key; size_t len;
    make_key(v, &key, &len);
    pthread_mutex_lock(&s->lock);
    int exists = zend_hash_str_exists(&s->ht, key, len);
    pthread_mutex_unlock(&s->lock);
    efree(key);
    RETURN_BOOL(exists);
}

PHP_METHOD(Set, remove)
{
    zval *v;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_ZVAL(v) ZEND_PARSE_PARAMETERS_END();
    gcd_set *s = set_fetch(Z_OBJ_P(ZEND_THIS))->s;
    char *key; size_t len;
    make_key(v, &key, &len);
    pthread_mutex_lock(&s->lock);
    int removed = zend_hash_str_del(&s->ht, key, len) == SUCCESS;
    pthread_mutex_unlock(&s->lock);
    efree(key);
    RETURN_BOOL(removed);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_count, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Set, count)
{
    ZEND_PARSE_PARAMETERS_NONE();
    gcd_set *s = set_fetch(Z_OBJ_P(ZEND_THIS))->s;
    pthread_mutex_lock(&s->lock);
    zend_long c = zend_hash_num_elements(&s->ht);
    pthread_mutex_unlock(&s->lock);
    RETURN_LONG(c);
}

static const zend_function_entry set_methods[] = {
    PHP_ME(Set, __construct, arginfo_set_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Set, open, arginfo_set_open, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(Set, add, arginfo_set_value, ZEND_ACC_PUBLIC)
    PHP_ME(Set, contains, arginfo_set_value, ZEND_ACC_PUBLIC)
    PHP_ME(Set, remove, arginfo_set_value, ZEND_ACC_PUBLIC)
    PHP_ME(Set, count, arginfo_set_count, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void gcd_set_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Set", set_methods);
    gcd_set_ce = zend_register_internal_class(&ce);
    gcd_set_ce->create_object = set_create_object;
    memcpy(&set_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    set_handlers.offset = XtOffsetOf(gcd_set_object, std);
    set_handlers.free_obj = set_free_object;
    zend_hash_init(&g_registry, 8, NULL, NULL, 1);
}