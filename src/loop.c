#include "loop.h"
#include "runtime/thread_pool.h"

ZEND_BEGIN_ARG_INFO_EX(arginfo_drain, 0, 0, 0)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

PHP_FUNCTION(drain)
{
    double timeout = -1;
    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END();
    gcd_pool_drain(timeout);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_main, 0, 0, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(main)
{
    ZEND_PARSE_PARAMETERS_NONE();
    /* v1: drain all pending work then return. A true never-returning,
     * source-driven dispatch_main arrives with US4 event sources. */
    gcd_pool_drain(-1);
}