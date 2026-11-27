#include "source.h"
#include "../queue/queue.h"
#include "../runtime/task.h"
#include "../runtime/thread_pool.h"
#include "../runtime/copy.h"
#include <Zend/zend_exceptions.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdatomic.h>

#define GCD_SOURCE_TIMER 1
#define GCD_SOURCE_SIGNAL 2
#define GCD_SOURCE_READ 3
#define GCD_SOURCE_WRITE 4
#define GCD_SOURCE_PROC 5
#define GCD_SOURCE_DATA_ADD 6

typedef struct {
    int type;
    int handle;                 /* fd (read/write) or signo (signal) */
    zend_op_array *handler;     /* persistent, source-owned */
    gcd_queue *queue;
    double start, interval;
    volatile int cancelled;
    pthread_t thread;
    int running;
    atomic_long data;           /* data source accumulator */
    pthread_mutex_t data_mutex;
    pthread_cond_t data_cond;
} gcd_source;

typedef struct {
    gcd_source *s;
    zend_object std;
} gcd_source_object;

static zend_class_entry *gcd_source_ce;
static zend_object_handlers source_handlers;

static inline gcd_source_object *source_fetch(zend_object *obj)
{
    return (gcd_source_object *) ((char *) obj - XtOffsetOf(gcd_source_object, std));
}

static void sleep_sec(double s)
{
    if (s <= 0) return;
    struct timespec ts;
    ts.tv_sec = (time_t) s;
    ts.tv_nsec = (long) ((s - (time_t) s) * 1e9);
    nanosleep(&ts, NULL);
}

static void dispatch_handler(gcd_source *s)
{
    if (s->handler && s->queue) {
        gcd_task *t = gcd_task_create(s->handler, NULL, 0);
        t->owns_oa = 0;          /* handler owned by source */
        t->queue = s->queue;
        gcd_queue_retain(s->queue);
        t->is_sync = 0;
        if (s->queue->width == 1) gcd_queue_enqueue(s->queue, t);
        else gcd_pool_submit(t);
    }
}

static void *source_thread(void *arg)
{
    gcd_source *s = arg;
    if (s->type == GCD_SOURCE_TIMER) {
        sleep_sec(s->start);
        while (!s->cancelled) {
            dispatch_handler(s);
            sleep_sec(s->interval);
        }
    } else if (s->type == GCD_SOURCE_SIGNAL) {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, s->handle);
        pthread_sigmask(SIG_BLOCK, &mask, NULL);
        int sig;
        while (!s->cancelled) {
            if (sigwait(&mask, &sig) == 0 && !s->cancelled) {
                dispatch_handler(s);
            }
        }
    } else if (s->type == GCD_SOURCE_PROC) {
        while (!s->cancelled) {
            int status;
            pid_t r = waitpid(s->handle, &status, WNOHANG);
            if (r == s->handle) {
                dispatch_handler(s);
                break;
            }
            usleep(10000);
        }
    } else if (s->type == GCD_SOURCE_DATA_ADD) {
        pthread_mutex_lock(&s->data_mutex);
        while (!s->cancelled) {
            while (atomic_load(&s->data) == 0 && !s->cancelled) {
                pthread_cond_wait(&s->data_cond, &s->data_mutex);
            }
            if (s->cancelled) break;
            long d = atomic_exchange(&s->data, 0);
            pthread_mutex_unlock(&s->data_mutex);
            if (s->handler && s->queue) {
                /* source thread has no PHP context: build argv persistently */
                zval argv;
                HashTable *ht = pemalloc(sizeof(HashTable), 1);
                zend_hash_init(ht, 1, NULL, NULL, 1);
                ZVAL_ARR(&argv, ht);
                zval dv;
                ZVAL_LONG(&dv, d);
                zend_hash_index_update(ht, 0, &dv);
                gcd_task *t = gcd_task_create(s->handler, &argv, 1);
                /* task takes ownership of argv (shallow copy); do not free here */
                t->owns_oa = 0;
                t->queue = s->queue;
        gcd_queue_retain(s->queue);
                t->is_sync = 0;
                if (s->queue->width == 1) gcd_queue_enqueue(s->queue, t);
                else gcd_pool_submit(t);
            }
            pthread_mutex_lock(&s->data_mutex);
        }
        pthread_mutex_unlock(&s->data_mutex);
    } else { /* READ / WRITE */
        struct pollfd pfd;
        pfd.fd = s->handle;
        pfd.events = (s->type == GCD_SOURCE_READ) ? POLLIN : POLLOUT;
        while (!s->cancelled) {
            int rc = poll(&pfd, 1, 100);
            if (rc > 0 && (pfd.revents & pfd.events)) {
                dispatch_handler(s);
            }
        }
    }
    return NULL;
}

static zend_object *source_create_object(zend_class_entry *ce)
{
    gcd_source_object *o = ecalloc(1, sizeof(gcd_source_object) + zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &source_handlers;
    o->s = calloc(1, sizeof(gcd_source));
    o->s->interval = 0;
    atomic_init(&o->s->data, 0);
    pthread_mutex_init(&o->s->data_mutex, NULL);
    pthread_cond_init(&o->s->data_cond, NULL);
    return &o->std;
}

static void source_free_object(zend_object *object)
{
    gcd_source_object *o = source_fetch(object);
    if (o->s) {
        if (o->s->running) {
            o->s->cancelled = 1;
            if (o->s->type == GCD_SOURCE_SIGNAL) {
                pthread_kill(o->s->thread, o->s->handle);
            }
            if (o->s->type == GCD_SOURCE_DATA_ADD) {
                pthread_mutex_lock(&o->s->data_mutex);
                pthread_cond_signal(&o->s->data_cond);
                pthread_mutex_unlock(&o->s->data_mutex);
            }
            pthread_join(o->s->thread, NULL);
        }
        /* wait for borrowed-handler tasks to finish before freeing the handler */
        gcd_pool_drain(-1);
        if (o->s->handler) gcd_op_array_free_persistent(o->s->handler);
        pthread_mutex_destroy(&o->s->data_mutex);
        pthread_cond_destroy(&o->s->data_cond);
        free(o->s);
    }
    zend_object_std_dtor(object);
}

static gcd_source *source_from_zval(zval *zv)
{
    if (Z_TYPE_P(zv) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(zv), gcd_source_ce)) {
        return NULL;
    }
    return source_fetch(Z_OBJ_P(zv))->s;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_source_create, 0, 0, 1)
    ZEND_ARG_INFO(0, type)
    ZEND_ARG_INFO(0, handle)
    ZEND_ARG_INFO(0, mask)
    ZEND_ARG_INFO(0, queue)
ZEND_END_ARG_INFO()

PHP_FUNCTION(source_create)
{
    zend_long type, handle = 0, mask = 0;
    zval *qobj = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 4)
        Z_PARAM_LONG(type)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(handle)
        Z_PARAM_LONG(mask)
        Z_PARAM_OBJECT(qobj)
    ZEND_PARSE_PARAMETERS_END();
    if (type != GCD_SOURCE_TIMER && type != GCD_SOURCE_SIGNAL
        && type != GCD_SOURCE_READ && type != GCD_SOURCE_WRITE
        && type != GCD_SOURCE_PROC && type != GCD_SOURCE_DATA_ADD) {
        zend_throw_exception(zend_ce_value_error, "unsupported source type in v1", 0);
        RETURN_THROWS();
    }
    object_init_ex(return_value, gcd_source_ce);
    gcd_source *s = source_fetch(Z_OBJ_P(return_value))->s;
    s->type = (int) type;
    s->handle = (int) handle;
    s->queue = qobj ? gcd_queue_from_zval(qobj) : NULL;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_source_handler, 0, 0, 2)
    ZEND_ARG_INFO(0, source)
    ZEND_ARG_INFO(0, handler)
ZEND_END_ARG_INFO()

PHP_FUNCTION(source_set_event_handler)
{
    zval *sobj, *closure;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT(sobj)
        Z_PARAM_OBJECT(closure)
    ZEND_PARSE_PARAMETERS_END();
    gcd_source *s = source_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Source", 0); RETURN_THROWS(); }
    zend_op_array *src = gcd_get_closure_oa(closure);
    if (!src) { zend_throw_exception(zend_ce_type_error, "handler must be a user-function Closure", 0); RETURN_THROWS(); }
    zend_op_array *copy = gcd_op_array_copy_persistent(src);
    if (!copy) { zend_throw_exception(gcd_capture_ce, "handler unsupported in v1", 0); RETURN_THROWS(); }
    if (s->handler) gcd_op_array_free_persistent(s->handler);
    s->handler = copy;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_source_set_timer, 0, 0, 2)
    ZEND_ARG_INFO(0, source)
    ZEND_ARG_INFO(0, start)
    ZEND_ARG_INFO(0, interval)
    ZEND_ARG_INFO(0, leeway)
ZEND_END_ARG_INFO()

PHP_FUNCTION(source_set_timer)
{
    zval *sobj;
    double start, interval, leeway = 0;
    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_OBJECT(sobj)
        Z_PARAM_DOUBLE(start)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(interval)
        Z_PARAM_DOUBLE(leeway)
    ZEND_PARSE_PARAMETERS_END();
    gcd_source *s = source_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Source", 0); RETURN_THROWS(); }
    s->start = start;
    s->interval = interval;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_source_one, 0, 0, 1)
    ZEND_ARG_INFO(0, source)
ZEND_END_ARG_INFO()

PHP_FUNCTION(source_resume)
{
    zval *sobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(sobj) ZEND_PARSE_PARAMETERS_END();
    gcd_source *s = source_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Source", 0); RETURN_THROWS(); }
    if (!s->running) {
        if (s->type == GCD_SOURCE_SIGNAL) {
            /* block in calling thread so spawned workers inherit the mask and
             * the signal is routed to sigwait rather than default action */
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, s->handle);
            pthread_sigmask(SIG_BLOCK, &mask, NULL);
        }
        s->cancelled = 0;
        s->running = 1;
        pthread_create(&s->thread, NULL, source_thread, s);
    }
}

PHP_FUNCTION(source_cancel)
{
    zval *sobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(sobj) ZEND_PARSE_PARAMETERS_END();
    gcd_source *s = source_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Source", 0); RETURN_THROWS(); }
    if (s->running) {
        s->cancelled = 1;
        if (s->type == GCD_SOURCE_SIGNAL) {
            pthread_kill(s->thread, s->handle);   /* wake sigwait */
        }
        if (s->type == GCD_SOURCE_DATA_ADD) {
            pthread_mutex_lock(&s->data_mutex);
            pthread_cond_signal(&s->data_cond);
            pthread_mutex_unlock(&s->data_mutex);
        }
        pthread_join(s->thread, NULL);
        s->running = 0;
    }
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_source_merge_data, 0, 0, 2)
    ZEND_ARG_INFO(0, source)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

PHP_FUNCTION(source_merge_data)
{
    zval *sobj;
    zend_long val;
    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT(sobj)
        Z_PARAM_LONG(val)
    ZEND_PARSE_PARAMETERS_END();
    gcd_source *s = source_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Source", 0); RETURN_THROWS(); }
    pthread_mutex_lock(&s->data_mutex);
    atomic_fetch_add(&s->data, val);
    pthread_cond_signal(&s->data_cond);
    pthread_mutex_unlock(&s->data_mutex);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_source_get_data, 0, 0, 1)
    ZEND_ARG_INFO(0, source)
ZEND_END_ARG_INFO()

PHP_FUNCTION(source_get_data)
{
    zval *sobj;
    ZEND_PARSE_PARAMETERS_START(1, 1) Z_PARAM_OBJECT(sobj) ZEND_PARSE_PARAMETERS_END();
    gcd_source *s = source_from_zval(sobj);
    if (!s) { zend_throw_exception(zend_ce_type_error, "expected GCD\\Source", 0); RETURN_THROWS(); }
    RETURN_LONG((zend_long) atomic_load(&s->data));
}

void gcd_source_minit(void)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "GCD\\Source", NULL);
    gcd_source_ce = zend_register_internal_class(&ce);
    gcd_source_ce->create_object = source_create_object;
    memcpy(&source_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    source_handlers.offset = XtOffsetOf(gcd_source_object, std);
    source_handlers.free_obj = source_free_object;

    zend_register_long_constant("GCD\\SOURCE_TIMER", sizeof("GCD\\SOURCE_TIMER") - 1, GCD_SOURCE_TIMER, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\SOURCE_SIGNAL", sizeof("GCD\\SOURCE_SIGNAL") - 1, GCD_SOURCE_SIGNAL, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\SOURCE_READ", sizeof("GCD\\SOURCE_READ") - 1, GCD_SOURCE_READ, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\SOURCE_WRITE", sizeof("GCD\\SOURCE_WRITE") - 1, GCD_SOURCE_WRITE, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\SOURCE_PROC", sizeof("GCD\\SOURCE_PROC") - 1, GCD_SOURCE_PROC, CONST_CS | CONST_PERSISTENT, 0);
    zend_register_long_constant("GCD\\SOURCE_DATA_ADD", sizeof("GCD\\SOURCE_DATA_ADD") - 1, GCD_SOURCE_DATA_ADD, CONST_CS | CONST_PERSISTENT, 0);
}