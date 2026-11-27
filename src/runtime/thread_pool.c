#include "thread_pool.h"
#include "php_context.h"
#include <stdlib.h>

#define GCD_POOL_DEFAULT_SIZE 4

static pthread_t *g_threads;
static int g_size;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static gcd_task *g_head, *g_tail;
static int g_shutdown;
static int g_inited;

/* boot handshake: all workers boot before any task dispatches */
static pthread_mutex_t g_boot_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_boot_cond = PTHREAD_COND_INITIALIZER;
static int g_booted;
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *worker_main(void *arg)
{
    (void) arg;
    int ok = (gcd_context_boot() == SUCCESS);
    pthread_mutex_lock(&g_boot_mutex);
    g_booted++;
    pthread_cond_signal(&g_boot_cond);
    pthread_mutex_unlock(&g_boot_mutex);
    if (!ok) return NULL;

    for (;;) {
        pthread_mutex_lock(&g_mutex);
        while (!g_head && !g_shutdown) {
            pthread_cond_wait(&g_cond, &g_mutex);
        }
        if (g_shutdown && !g_head) {
            pthread_mutex_unlock(&g_mutex);
            break;
        }
        gcd_task *t = g_head;
        g_head = t->next;
        if (!g_head) g_tail = NULL;
        pthread_mutex_unlock(&g_mutex);

        gcd_task_execute(t);
    }
    gcd_context_shutdown();
    return NULL;
}

static void pool_init(void)
{
    pthread_mutex_lock(&g_init_mutex);
    if (g_inited) {
        pthread_mutex_unlock(&g_init_mutex);
        return;
    }
    g_size = GCD_POOL_DEFAULT_SIZE;
    g_threads = malloc(sizeof(pthread_t) * g_size);
    g_head = g_tail = NULL;
    g_shutdown = 0;
    g_booted = 0;
    for (int i = 0; i < g_size; i++) {
        pthread_create(&g_threads[i], NULL, worker_main, NULL);
    }
    /* wait until every worker has booted its PHP context */
    pthread_mutex_lock(&g_boot_mutex);
    while (g_booted < g_size) {
        pthread_cond_wait(&g_boot_cond, &g_boot_mutex);
    }
    pthread_mutex_unlock(&g_boot_mutex);
    g_inited = 1;
    pthread_mutex_unlock(&g_init_mutex);
}

void gcd_pool_submit(gcd_task *t)
{
    pool_init();
    pthread_mutex_lock(&g_mutex);
    t->next = NULL;
    if (g_tail) g_tail->next = t; else g_head = t;
    g_tail = t;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
}

void gcd_pool_shutdown(void)
{
    if (!g_inited) return;
    pthread_mutex_lock(&g_mutex);
    g_shutdown = 1;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_mutex);
    for (int i = 0; i < g_size; i++) {
        pthread_join(g_threads[i], NULL);
    }
    free(g_threads);
    g_inited = 0;
}