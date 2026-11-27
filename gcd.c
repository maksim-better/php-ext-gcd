#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_gcd.h"
#include "src/queue/queue.h"
#include "src/queue/dispatch.h"
#include "src/queue/group.h"
#include "src/queue/semaphore.h"
#include "src/queue/once.h"
#include "src/runtime/thread_pool.h"
#include "src/runtime/php_context.h"
#include "src/runtime/error.h"
#include "src/container/atomic.h"
#include "src/container/cqueue.h"
#include "src/container/set.h"
#include "src/container/map.h"
#include <ext/standard/info.h>

TSRMLS_CACHE_DEFINE();

PHP_MINIT_FUNCTION(gcd)
{
    gcd_context_capture_parent();
    gcd_error_minit();
    gcd_queue_minit();
    gcd_group_minit();
    gcd_semaphore_minit();
    gcd_once_minit();
    gcd_atomic_minit();
    gcd_cqueue_minit();
    gcd_set_minit();
    gcd_map_minit();
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(gcd)
{
    gcd_pool_shutdown();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(gcd)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "gcd support", "enabled");
    php_info_print_table_row(2, "version", PHP_GCD_VERSION);
    php_info_print_table_row(2, "thread safety", "ZTS required");
    php_info_print_table_end();
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_queue_create, 0, 0, 1)
    ZEND_ARG_INFO(0, label)
    ZEND_ARG_INFO(0, width)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_global_queue, 0, 0, 0)
    ZEND_ARG_INFO(0, qos)
    ZEND_ARG_INFO(0, pool_size)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_dispatch, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, task)
    ZEND_ARG_INFO(0, argv)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_group_create, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_group_one, 0, 0, 1)
    ZEND_ARG_INFO(0, group)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_group_wait, 0, 0, 1)
    ZEND_ARG_INFO(0, group)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_group_async, 0, 0, 3)
    ZEND_ARG_INFO(0, group)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, task)
    ZEND_ARG_INFO(0, argv)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_sem_create, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_sem_open, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_sem_wait, 0, 0, 1)
    ZEND_ARG_INFO(0, semaphore)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_sem_signal, 0, 0, 1)
    ZEND_ARG_INFO(0, semaphore)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_once, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, task)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_queue_set_specific, 0, 0, 3)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_queue_get_specific, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_set_error_handler, 0, 0, 1)
    ZEND_ARG_INFO(0, handler)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_gcd_queue_set_error_handler, 0, 0, 2)
    ZEND_ARG_INFO(0, queue)
    ZEND_ARG_INFO(0, handler)
ZEND_END_ARG_INFO()

static const zend_function_entry gcd_functions[] = {
    ZEND_NS_FE("GCD", queue_create, arginfo_gcd_queue_create)
    ZEND_NS_FE("GCD", global_queue, arginfo_gcd_global_queue)
    ZEND_NS_FE("GCD", queue_suspend, arginfo_gcd_group_one)
    ZEND_NS_FE("GCD", queue_resume, arginfo_gcd_group_one)
    ZEND_NS_FE("GCD", queue_set_specific, arginfo_gcd_queue_set_specific)
    ZEND_NS_FE("GCD", queue_get_specific, arginfo_gcd_queue_get_specific)
    ZEND_NS_FE("GCD", queue_set_error_handler, arginfo_gcd_queue_set_error_handler)
    ZEND_NS_FE("GCD", set_error_handler, arginfo_gcd_set_error_handler)
    ZEND_NS_FE("GCD", sync, arginfo_gcd_dispatch)
    ZEND_NS_FE("GCD", async, arginfo_gcd_dispatch)
    ZEND_NS_FE("GCD", barrier_async, arginfo_gcd_dispatch)
    ZEND_NS_FE("GCD", barrier_sync, arginfo_gcd_dispatch)
    ZEND_NS_FE("GCD", group_create, arginfo_gcd_group_create)
    ZEND_NS_FE("GCD", group_enter, arginfo_gcd_group_one)
    ZEND_NS_FE("GCD", group_leave, arginfo_gcd_group_one)
    ZEND_NS_FE("GCD", group_wait, arginfo_gcd_group_wait)
    ZEND_NS_FE("GCD", group_async, arginfo_gcd_group_async)
    ZEND_NS_FE("GCD", semaphore_create, arginfo_gcd_sem_create)
    ZEND_NS_FE("GCD", semaphore_open, arginfo_gcd_sem_open)
    ZEND_NS_FE("GCD", semaphore_wait, arginfo_gcd_sem_wait)
    ZEND_NS_FE("GCD", semaphore_signal, arginfo_gcd_sem_signal)
    ZEND_NS_FE("GCD", once, arginfo_gcd_once)
    PHP_FE_END
};

zend_module_entry gcd_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_GCD_EXTNAME,
    gcd_functions,
    PHP_MINIT(gcd),
    PHP_MSHUTDOWN(gcd),
    NULL,
    NULL,
    PHP_MINFO(gcd),
    PHP_GCD_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_GCD
ZEND_GET_MODULE(gcd)
#endif