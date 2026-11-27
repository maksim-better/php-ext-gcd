#include "queue.h"
#include <Zend/zend_exceptions.h>
#include <stdlib.h>
#include <string.h>

zend_class_entry *gcd_queue_ce;
zend_class_entry *gcd_capture_ce;
static zend_object_handlers gcd_queue_handlers;

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
    obj->q = calloc(1, sizeof(gcd_queue));
    obj->q->width = 1;
    pthread_mutex_init(&obj->q->drain_lock, NULL);
    return &obj->std;
}

static void gcd_queue_free_object(zend_object *object)
{
    gcd_queue_object *obj = gcd_queue_fetch(object);
    if (obj->q) {
        pthread_mutex_destroy(&obj->q->drain_lock);
        free(obj->q->label);
        free(obj->q);
    }
    zend_object_std_dtor(object);
}

gcd_queue *gcd_queue_from_zval(zval *zv)
{
    if (Z_TYPE_P(zv) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(zv), gcd_queue_ce)) {
        return NULL;
    }
    return gcd_queue_fetch(Z_OBJ_P(zv))->q;
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