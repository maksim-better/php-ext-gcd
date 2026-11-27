/* S3 spike (T010): cross-thread value boundary via serialize into PERSISTENT
 * storage. Main thread serializes an array (request-local emalloc), copies to
 * pemalloc; worker thread unserializes into its own request memory and reads
 * structure intact. Validates the v1 serializable-value boundary. */
#include <sapi/embed/php_embed.h>
#include <pthread.h>
#include <stdio.h>
#include <ext/standard/php_var.h>
#include <ext/standard/php_smart_string.h>

TSRMLS_CACHE_DEFINE();

static unsigned char *g_buf;
static size_t g_len;

static void *worker(void *arg)
{
    (void) arg;
    ts_resource(0);
    TSRMLS_CACHE_UPDATE();
    php_request_startup();

    zval zv;
    const unsigned char *p = g_buf, *max = g_buf + g_len;
    php_unserialize_data_t ud = php_var_unserialize_init();
    if (php_var_unserialize(&zv, &p, max, &ud) && Z_TYPE(zv) == IS_ARRAY) {
        zval *xp = zend_hash_str_find(Z_ARRVAL(zv), "x", sizeof("x") - 1);
        zval *np = zend_hash_str_find(Z_ARRVAL(zv), "name", sizeof("name") - 1);
        printf("worker-got x=%ld name=%s\n", Z_LVAL_P(xp), Z_STRVAL_P(np));
    } else {
        printf("worker-unserialize-FAILED\n");
    }
    php_var_unserialize_destroy(ud);
    zval_ptr_dtor(&zv);

    php_request_shutdown(NULL);
    ts_free_thread();
    return NULL;
}

int main(void)
{
    if (php_embed_init(0, NULL) != SUCCESS) return 1;

    zval arr;
    array_init(&arr);
    add_assoc_long(&arr, "x", 42);
    add_assoc_string(&arr, "name", "gcd");

    smart_str buf = {0};
    php_serialize_data_t sd = php_var_serialize_init();
    php_var_serialize(&buf, &arr, &sd);
    php_var_serialize_destroy(sd);

    g_len = ZSTR_LEN(buf.s);
    g_buf = pemalloc(g_len, 1);          /* persistent: survives across threads */
    memcpy(g_buf, ZSTR_VAL(buf.s), g_len);
    zend_string_release(buf.s);
    zval_ptr_dtor(&arr);

    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);
    pthread_join(t, NULL);

    pefree(g_buf, 1);
    php_embed_shutdown();
    printf("s3-done\n");
    return 0;
}