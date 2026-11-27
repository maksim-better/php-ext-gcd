/* S1 spike (T008): validate per-thread ZTS PHP runtime boot on PHP 8.4.
 * Mechanism under test (research R1): ts_resource(0) + TSRMLS_CACHE_UPDATE()
 * + php_request_startup() on a fresh pthread, then run PHP code, then teardown.
 * Exploratory spike — not production code. */
#include <sapi/embed/php_embed.h>
#include <pthread.h>
#include <stdio.h>

/* 定义本编译单元的线程局部 TSRMLS 缓存（_tsrm_ls_cache） */
TSRMLS_CACHE_DEFINE();

static void *worker(void *arg)
{
    (void)arg;
    ts_resource(0);              /* allocate this thread's TSRM context */
    TSRMLS_CACHE_UPDATE();       /* point thread-local cache at it */

    if (php_request_startup() == SUCCESS) {
        zend_eval_string("echo \"worker-thread-ok\\n\";", NULL, (char *) "s1-worker");
        php_request_shutdown(NULL);
    } else {
        fprintf(stderr, "s1: php_request_startup FAILED\n");
    }
    ts_free_thread();
    return NULL;
}

int main(void)
{
    if (php_embed_init(0, NULL) != SUCCESS) {
        fprintf(stderr, "s1: php_embed_init FAILED\n");
        return 1;
    }
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) {
        fprintf(stderr, "s1: pthread_create FAILED\n");
        return 1;
    }
    pthread_join(t, NULL);
    php_embed_shutdown();
    printf("s1-done\n");
    return 0;
}