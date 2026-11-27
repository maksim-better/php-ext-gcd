#include "php_context.h"
#include <main/php.h>
#include <main/php_main.h>
#include <main/SAPI.h>
#include <main/php_globals.h>
#include <Zend/zend_globals.h>
#include <pthread.h>

TSRMLS_CACHE_EXTERN();

/* Serialize request startup across workers: concurrent RINIT (zend_activate_modules)
 * races on process-global module state. */
static pthread_mutex_t boot_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Parent (main thread) SAPI globals, captured at MINIT and copied into workers
 * (mirrors parallel's scheduler setup). */
static int parent_argc;
static char **parent_argv;

void gcd_context_capture_parent(void)
{
    parent_argc = SG(request_info).argc;
    parent_argv = SG(request_info).argv;
}

int gcd_context_boot(void)
{
    ts_resource(0);
    TSRMLS_CACHE_UPDATE();

    pthread_mutex_lock(&boot_mutex);
    SG(request_info).argc = parent_argc;
    SG(request_info).argv = parent_argv;
    PG(expose_php) = 0;
    PG(auto_globals_jit) = 1;
    PG(enable_dl) = 0;

    if (php_request_startup() == FAILURE) {
        pthread_mutex_unlock(&boot_mutex);
        return FAILURE;
    }
    PG(during_request_startup) = 0;
    SG(sapi_started) = 0;
    SG(headers_sent) = 1;
    SG(request_info).no_headers = 1;
    pthread_mutex_unlock(&boot_mutex);
    return SUCCESS;
}

void gcd_context_shutdown(void)
{
    gc_collect_cycles();
    php_request_shutdown(NULL);
    ts_free_thread();
}