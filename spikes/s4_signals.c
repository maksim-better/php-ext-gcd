/* S4 spike (T011): signal ownership coexisting with worker threads.
 * Main thread installs SIGPIPE=SIG_IGN + a SIGUSR1 handler; workers run PHP
 * concurrently; main raises SIGUSR1. Validates handlers are process-wide and
 * coexist with per-thread PHP runtimes without crash. */
#include <sapi/embed/php_embed.h>
#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

TSRMLS_CACHE_DEFINE();

static volatile sig_atomic_t g_usr = 0;
static void on_usr(int s) { (void) s; g_usr++; }

static void *worker(void *a)
{
    (void) a;
    ts_resource(0);
    TSRMLS_CACHE_UPDATE();
    php_request_startup();
    zend_eval_string("usleep(20000);", NULL, (char *) "s4");
    php_request_shutdown(NULL);
    ts_free_thread();
    return NULL;
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, on_usr);
    if (php_embed_init(0, NULL) != SUCCESS) return 1;

    pthread_t t[4];
    for (int i = 0; i < 4; i++) pthread_create(&t[i], NULL, worker, NULL);
    for (int i = 0; i < 3; i++) { kill(getpid(), SIGUSR1); usleep(2000); }
    for (int i = 0; i < 4; i++) pthread_join(t[i], NULL);

    php_embed_shutdown();
    printf("s4 usr1=%d done\n", (int) g_usr);
    return 0;
}