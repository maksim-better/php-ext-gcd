/* S2 spike (T009): N threads each boot a ZTS PHP context and run curl
 * (own handle per thread) against a local server. Validates the crawler
 * HTTP hard risk: no crash/leak, CURLOPT_NOSIGNAL auto-set in ZTS. */
#include <sapi/embed/php_embed.h>
#include <pthread.h>
#include <stdio.h>

TSRMLS_CACHE_DEFINE();

#define NTHREADS 8

static void *worker(void *arg)
{
    long id = (long) arg;
    ts_resource(0);
    TSRMLS_CACHE_UPDATE();
    if (php_request_startup() != SUCCESS) {
        fprintf(stderr, "s2: startup fail\n");
        return NULL;
    }
    char code[512];
    snprintf(code, sizeof(code),
        "$ch=curl_init('http://127.0.0.1:8099/');"
        "curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);"
        "curl_setopt($ch,CURLOPT_TIMEOUT,5);"
        "$r=curl_exec($ch);$e=curl_errno($ch);curl_close($ch);"
        "echo \"curl-ok-%ld errno=$e len=\".strlen((string)$r).\"\\n\";", id);
    zend_eval_string(code, NULL, (char *) "s2");
    php_request_shutdown(NULL);
    ts_free_thread();
    return NULL;
}

int main(void)
{
    if (php_embed_init(0, NULL) != SUCCESS) return 1;
    pthread_t t[NTHREADS];
    for (long i = 0; i < NTHREADS; i++) pthread_create(&t[i], NULL, worker, (void *) i);
    for (int i = 0; i < NTHREADS; i++) pthread_join(t[i], NULL);
    php_embed_shutdown();
    printf("s2-done\n");
    return 0;
}