#include "thread_pool.h"
#include "php_context.h"
#include "../queue/queue.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#define GCD_POOL_DEFAULT_SIZE 4

static atomic_long g_pending;
static pthread_mutex_t g_pending_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_pending_cond = PTHREAD_COND_INITIALIZER;

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

/* Process-wide set of signals owned by SIGNAL sources; every worker and source
 * thread must block them so delivery goes to sigwait, not default action. */
static sigset_t g_source_sigset;
static int g_sigset_inited = 0;
static atomic_long g_sig_gen;

void gcd_pool_block_source_signal(int signo)
{
    pthread_mutex_lock(&g_mutex);
    if (!g_sigset_inited) { sigemptyset(&g_source_sigset); g_sigset_inited = 1; }
    sigaddset(&g_source_sigset, signo);
    atomic_fetch_add(&g_sig_gen, 1);
    pthread_cond_broadcast(&g_cond);   /* wake idle workers to re-mask */
    pthread_mutex_unlock(&g_mutex);
}

void gcd_pool_apply_source_mask(void)
{
    pthread_mutex_lock(&g_mutex);
    if (g_sigset_inited) pthread_sigmask(SIG_BLOCK, &g_source_sigset, NULL);
    pthread_mutex_unlock(&g_mutex);
}

static void *worker_main(void *arg)
{
    (void) arg;
    int ok = (gcd_context_boot() == SUCCESS);
    pthread_mutex_lock(&g_boot_mutex);
    g_booted++;
    pthread_cond_signal(&g_boot_cond);
    pthread_mutex_unlock(&g_boot_mutex);
    if (!ok) return NULL;

    uint64_t t_sig_gen = 0;
    for (;;) {
        pthread_mutex_lock(&g_mutex);
        for (;;) {
            if (t_sig_gen != (uint64_t) atomic_load(&g_sig_gen)) {
                if (g_sigset_inited) pthread_sigmask(SIG_BLOCK, &g_source_sigset, NULL);
                t_sig_gen = (uint64_t) atomic_load(&g_sig_gen);
            }
            if (g_head || g_shutdown) break;
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

        if (t->op_array == NULL) {
            gcd_queue_drain(t->queue);   /* serial drain job */
            gcd_queue_release(t->queue);
            pefree(t, 1);
        } else {
            gcd_task_execute(t);
        }
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
    if (t->op_array) atomic_fetch_add(&g_pending, 1);
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

void gcd_pool_pending_inc(void)
{
    atomic_fetch_add(&g_pending, 1);
}

void gcd_pool_task_done(void)
{
    long old = atomic_fetch_sub(&g_pending, 1);
    if (old == 1) {
        pthread_mutex_lock(&g_pending_mutex);
        pthread_cond_broadcast(&g_pending_cond);
        pthread_mutex_unlock(&g_pending_mutex);
    }
}

void gcd_pool_drain(double timeout)
{
    pthread_mutex_lock(&g_pending_mutex);
    while (atomic_load(&g_pending) > 0) {
        if (timeout == 0) {
            break;
        } else if (timeout < 0) {
            pthread_cond_wait(&g_pending_cond, &g_pending_mutex);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += (time_t) timeout;
            ts.tv_nsec += (long) ((timeout - (time_t) timeout) * 1e9);
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
            int rc = pthread_cond_timedwait(&g_pending_cond, &g_pending_mutex, &ts);
            if (rc == ETIMEDOUT) break;
        }
    }
    pthread_mutex_unlock(&g_pending_mutex);
}