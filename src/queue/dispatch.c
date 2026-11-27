#include "dispatch.h"
#include "queue.h"
#include "../runtime/task.h"
#include "../runtime/copy.h"
#include "../runtime/thread_pool.h"
#include <Zend/zend_closures.h>
#include <Zend/zend_exceptions.h>

/* Build a persistent task from a Closure + argv. Throws on reject. */
static gcd_task *build_task(zval *closure, zval *argv)
{
    if (!instanceof_function(Z_OBJCE_P(closure), zend_ce_closure)) {
        zend_throw_exception(zend_ce_type_error, "task must be a Closure", 0);
        return NULL;
    }
    /* The closure's real op_array follows its zend_object (inverse of the public
     * ZEND_CLOSURE_OBJECT macro). zend_get_closure_invoke_method() returns an
     * internal __invoke wrapper, NOT this op_array. */
    zend_op_array *src_oa = (zend_op_array *) ((char *) Z_OBJ_P(closure) + sizeof(zend_object));
    zend_op_array *oa = gcd_op_array_copy_persistent(src_oa);
    if (!oa) {
        zend_throw_exception(gcd_capture_ce,
            "closure captures variables or uses statics/try-catch (unsupported in v1)", 0);
        return NULL;
    }
    zval pargv;
    ZVAL_NULL(&pargv);
    uint32_t argc = 0;
    if (argv) {
        gcd_zval_copy_persistent(&pargv, argv);
        argc = zend_hash_num_elements(Z_ARRVAL_P(argv));
    }
    return gcd_task_create(oa, &pargv, argc);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_dispatch, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, task)
    ZEND_ARG_INFO(0, argv)
ZEND_END_ARG_INFO()

PHP_FUNCTION(sync)
{
    zval *qobj, *closure, *argv = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(closure)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(argv)
    ZEND_PARSE_PARAMETERS_END();

    if (!gcd_queue_from_zval(qobj)) {
        zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0);
        RETURN_THROWS();
    }
    gcd_task *t = build_task(closure, argv);
    if (!t) RETURN_THROWS();

    gcd_pool_submit(t);
    gcd_task_wait(t);
    gcd_task_destroy(t);
    RETURN_NULL();
}

PHP_FUNCTION(async)
{
    zval *qobj, *closure, *argv = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(closure)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(argv)
    ZEND_PARSE_PARAMETERS_END();

    if (!gcd_queue_from_zval(qobj)) {
        zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0);
        RETURN_THROWS();
    }
    gcd_task *t = build_task(closure, argv);
    if (!t) RETURN_THROWS();

    gcd_pool_submit(t);
    /* v1 minimal: async leaks task until pool drain; refined with queue lifecycle */
    RETURN_NULL();
}