#include "error.h"
#include "copy.h"
#include "../queue/queue.h"
#include <Zend/zend_exceptions.h>

static zend_op_array *g_error_handler = NULL;

void gcd_error_set_global(zval *closure)
{
    zend_op_array *src = (zend_op_array *) ((char *) Z_OBJ_P(closure) + sizeof(zend_object));
    zend_op_array *copy = gcd_op_array_copy_persistent(src);
    if (g_error_handler) gcd_op_array_free_persistent(g_error_handler);
    g_error_handler = copy;
}

void gcd_error_report(gcd_task *t)
{
    if (!EG(exception)) return;
    zend_object *ex = EG(exception);
    zval rv;
    zval *msg = zend_read_property(zend_ce_exception, ex, "message", sizeof("message") - 1, 1, &rv);
    const char *msgstr = (msg && Z_TYPE_P(msg) == IS_STRING) ? Z_STRVAL_P(msg) : "";
    const char *cls = ZSTR_VAL(ex->ce->name);

    zend_op_array *h = (t->queue && t->queue->error_handler)
        ? t->queue->error_handler : g_error_handler;

    zval argv;
    array_init(&argv);
    add_next_index_string(&argv, msgstr);
    add_next_index_string(&argv, cls);
    zval pargv;
    gcd_zval_copy_persistent(&pargv, &argv);
    zval_ptr_dtor(&argv);

    zend_clear_exception();
    if (h) {
        gcd_invoke(h, &pargv, 2);
        if (EG(exception)) zend_clear_exception();
    }
    gcd_zval_free(&pargv, 1);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_error_handler, 0, 0, 1)
    ZEND_ARG_INFO(0, handler)
ZEND_END_ARG_INFO()

PHP_FUNCTION(set_error_handler)
{
    zval *closure;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(closure) ZEND_PARSE_PARAMETERS_END();
    if (!gcd_get_closure_oa(closure)) {
        zend_throw_exception(zend_ce_type_error, "handler must be a user-function Closure", 0);
        RETURN_THROWS();
    }
    gcd_error_set_global(closure);
}

void gcd_error_minit(void) {}