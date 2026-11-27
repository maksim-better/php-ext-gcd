#include "dispatch.h"
#include "queue.h"
#include "../runtime/task.h"
#include "../runtime/copy.h"
#include "../runtime/thread_pool.h"
#include <Zend/zend_closures.h>
#include <Zend/zend_exceptions.h>

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
    gcd_task *t = gcd_task_build(closure, argv);
    if (!t) RETURN_THROWS();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    extern __thread gcd_queue *gcd_current_queue;
    if (q && q->width == 1 && q == gcd_current_queue) {
        gcd_task_destroy(t);
        zend_throw_exception(gcd_deadlock_ce, "sync dispatch to current serial queue would deadlock", 0);
        RETURN_THROWS();
    }
    t->queue = q;
    t->is_sync = 1;

    if (q->width == 1) gcd_queue_enqueue(q, t); else gcd_pool_submit(t);
    gcd_task_wait(t);
    gcd_pool_task_done();
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
    gcd_task *t = gcd_task_build(closure, argv);
    if (!t) RETURN_THROWS();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    t->queue = q;
    t->is_sync = 0;

    if (q->width == 1) gcd_queue_enqueue(q, t); else gcd_pool_submit(t);
    RETURN_NULL();
}

PHP_FUNCTION(barrier_async)
{
    zval *qobj, *closure, *argv = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(closure)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(argv)
    ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    gcd_task *t = gcd_task_build(closure, argv);
    if (!t) RETURN_THROWS();
    t->queue = q;
    t->is_sync = 0;
    t->is_barrier = 1;
    if (q->width == 1) gcd_queue_enqueue(q, t); else gcd_pool_submit(t);
    RETURN_NULL();
}

PHP_FUNCTION(barrier_sync)
{
    zval *qobj, *closure, *argv = NULL;
    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_OBJECT(qobj)
        Z_PARAM_OBJECT(closure)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(argv)
    ZEND_PARSE_PARAMETERS_END();
    gcd_queue *q = gcd_queue_from_zval(qobj);
    if (!q) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Queue", 0); RETURN_THROWS(); }
    gcd_task *t = gcd_task_build(closure, argv);
    if (!t) RETURN_THROWS();
    t->queue = q;
    t->is_sync = 1;
    t->is_barrier = 1;
    if (q->width == 1) gcd_queue_enqueue(q, t); else gcd_pool_submit(t);
    gcd_task_wait(t);
    gcd_pool_task_done();
    gcd_task_destroy(t);
    RETURN_NULL();
}