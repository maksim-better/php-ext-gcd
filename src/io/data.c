#include "data.h"
#include <Zend/zend_exceptions.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *bytes;
    size_t len;
} gcd_data;

typedef struct {
    gcd_data *d;
    zend_object std;
} gcd_data_object;

static zend_class_entry *gcd_data_ce;
static zend_object_handlers data_handlers;

static inline gcd_data_object *data_fetch(zend_object *obj)
{
    return (gcd_data_object *) ((char *) obj - XtOffsetOf(gcd_data_object, std));
}

static zend_object *data_create_object(zend_class_entry *ce)
{
    gcd_data_object *o = ecalloc(1, sizeof(gcd_data_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &data_handlers;
    o->d = calloc(1, sizeof(gcd_data));
    return &o->std;
}

static void data_free_object(zend_object *object)
{
    gcd_data_object *o = data_fetch(object);
    if (o->d) {
        if (o->d->bytes) pefree(o->d->bytes, 1);
        free(o->d);
    }
    zend_object_std_dtor(object);
}

static gcd_data *data_from_zval(zval *zv)
{
    if (Z_TYPE_P(zv) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(zv), gcd_data_ce)) {
        return NULL;
    }
    return data_fetch(Z_OBJ_P(zv))->d;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_data_create, 0, 0, 1)
    ZEND_ARG_INFO(0, bytes)
ZEND_END_ARG_INFO()

PHP_FUNCTION(data_create)
{
    char *bytes; size_t len;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_STRING(bytes, len) ZEND_PARSE_PARAMETERS_END();
    object_init_ex(return_value, gcd_data_ce);
    gcd_data *d = data_fetch(Z_OBJ_P(return_value))->d;
    d->bytes = pemalloc(len ? len : 1, 1);
    memcpy(d->bytes, bytes, len);
    d->len = len;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_data_concat, 0, 0, 1)
    ZEND_ARG_VARIADIC_INFO(0, parts)
ZEND_END_ARG_INFO()

PHP_FUNCTION(data_concat)
{
    zval *args; int argc;
    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_VARIADIC('+', args, argc)
    ZEND_PARSE_PARAMETERS_END();
    size_t total = 0;
    for (int i = 0; i < argc; i++) {
        gcd_data *d = data_from_zval(&args[i]);
        if (d) total += d->len;
    }
    char *buf = pemalloc(total ? total : 1, 1);
    size_t off = 0;
    for (int i = 0; i < argc; i++) {
        gcd_data *d = data_from_zval(&args[i]);
        if (d) { memcpy(buf + off, d->bytes, d->len); off += d->len; }
    }
    object_init_ex(return_value, gcd_data_ce);
    gcd_data *nd = data_fetch(Z_OBJ_P(return_value))->d;
    nd->bytes = buf;
    nd->len = total;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_data_subrange, 0, 0, 3)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO(0, length)
ZEND_END_ARG_INFO()

PHP_FUNCTION(data_subrange)
{
    zval *dobj; zend_long offset, len;
    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_OBJECT(dobj)
        Z_PARAM_LONG(offset)
        Z_PARAM_LONG(len)
    ZEND_PARSE_PARAMETERS_END();
    gcd_data *d = data_from_zval(dobj);
    if (!d) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Data", 0); RETURN_THROWS(); }
    if (offset < 0) offset = 0;
    if ((size_t) offset > d->len) offset = d->len;
    if (len < 0 || (size_t)(offset + len) > d->len) len = d->len - offset;
    object_init_ex(return_value, gcd_data_ce);
    gcd_data *nd = data_fetch(Z_OBJ_P(return_value))->d;
    nd->bytes = pemalloc(len ? len : 1, 1);
    memcpy(nd->bytes, d->bytes + offset, len);
    nd->len = len;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_data_map, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

PHP_FUNCTION(data_create_map)
{
    zval *dobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(dobj) ZEND_PARSE_PARAMETERS_END();
    gcd_data *d = data_from_zval(dobj);
    if (!d) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Data", 0); RETURN_THROWS(); }
    RETURN_STRINGL(d->bytes, d->len);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_data_size, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_METHOD(Data, size)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long) data_fetch(Z_OBJ_P(ZEND_THIS))->d->len);
}

static const zend_function_entry data_methods[] = {
    PHP_ME(Data, size, arginfo_data_size, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void gcd_data_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Data", data_methods);
    gcd_data_ce = zend_register_internal_class(&ce);
    gcd_data_ce->create_object = data_create_object;
    memcpy(&data_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    data_handlers.offset = XtOffsetOf(gcd_data_object, std);
    data_handlers.free_obj = data_free_object;
}