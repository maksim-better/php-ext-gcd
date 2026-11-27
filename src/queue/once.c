#include "once.h"
#include <Zend/zend_exceptions.h>
#include <Zend/zend_execute.h>
#include <pthread.h>
#include <stdlib.h>

/* key -> state (0 none, 1 pending, 2 done) */
static HashTable g_once;
static pthread_mutex_t g_once_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_once_cond = PTHREAD_COND_INITIALIZER;

static int once_get_state(const char *key, size_t len)
{
    zend_long *p = zend_hash_str_find_ptr(&g_once, key, len);
    return p ? (int) *p : 0;
}

static void once_set_state(const char *key, size_t len, zend_long state)
{
    zend_long *p = zend_hash_str_find_ptr(&g_once, key, len);
    if (p) {
        *p = state;
    } else {
        zend_long *n = pemalloc(sizeof(zend_long), 1);
        *n = state;
        zend_hash_str_update_ptr(&g_once, key, len, n);
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_once, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, task)
ZEND_END_ARG_INFO()

PHP_FUNCTION(once)
{
    char *key; size_t len; zval *closure;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(key, len)
        Z_PARAM_OBJECT(closure)
    ZEND_PARSE_PARAMETERS_END();

    pthread_mutex_lock(&g_once_mutex);
    int state = once_get_state(key, len);
    while (state == 1) {
        pthread_cond_wait(&g_once_cond, &g_once_mutex);
        state = once_get_state(key, len);
    }
    if (state == 2) {
        pthread_mutex_unlock(&g_once_mutex);
        return;
    }
    once_set_state(key, len, 1);
    pthread_mutex_unlock(&g_once_mutex);

    /* execute the initializer in the current thread */
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    zval retval;
    ZVAL_UNDEF(&retval);
    if (zend_fcall_info_init(closure, 0, &fci, &fcc, NULL, NULL) == SUCCESS) {
        fci.retval = &retval;
        fci.param_count = 0;
        fci.params = NULL;
        zend_call_function(&fci, &fcc);
        zval_ptr_dtor(&retval);
        if (EG(exception)) zend_clear_exception();
    }

    pthread_mutex_lock(&g_once_mutex);
    once_set_state(key, len, 2);
    pthread_cond_broadcast(&g_once_cond);
    pthread_mutex_unlock(&g_once_mutex);
}

void gcd_once_minit(void)
{
    zend_hash_init(&g_once, 8, NULL, NULL, 1);
}