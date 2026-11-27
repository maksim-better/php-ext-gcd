#ifndef GCD_QUEUE_H
#define GCD_QUEUE_H

#include "php_gcd.h"
#include <pthread.h>
#include <stdatomic.h>

struct _gcd_task;

typedef struct _gcd_queue {
    char *label;
    int width;                 /* 1 = serial, >1 = concurrent */
    atomic_long refcount;      /* object + in-flight tasks + drain jobs */
    pthread_mutex_t drain_lock;
    pthread_mutex_t list_mutex;
    struct _gcd_task *head, *tail;  /* pending FIFO list (serial drain) */
    int draining;
    int inflight;                   /* concurrent width gate */
    int barrier_active;             /* exclusive barrier in progress */
    int suspended;
    HashTable specific;             /* persistent key -> zval context */
    zend_op_array *error_handler;   /* per-queue error handler (persistent) */
    int qos;                        /* -1 = inherit from target */
    struct _gcd_queue *target;      /* target queue (hierarchy) */
    pthread_mutex_t width_mutex;
    pthread_cond_t width_cond;
} gcd_queue;

typedef struct {
    gcd_queue *q;
    int owns;                  /* 1: free q on dtor; 0: borrowed wrapper */
    zend_object std;
} gcd_queue_object;

extern zend_class_entry *gcd_queue_ce;
extern zend_class_entry *gcd_capture_ce;
extern zend_class_entry *gcd_serialization_ce;
extern zend_class_entry *gcd_deadlock_ce;

void gcd_queue_minit(void);
gcd_queue *gcd_queue_from_zval(zval *zv);
void gcd_queue_retain(gcd_queue *q);
void gcd_queue_release(gcd_queue *q);
void gcd_queue_wrap_borrowed(zval *return_value, gcd_queue *q);
void gcd_queue_enqueue(gcd_queue *q, struct _gcd_task *t);
void gcd_queue_drain(gcd_queue *q);

PHP_FUNCTION(queue_create);
PHP_FUNCTION(global_queue);
PHP_FUNCTION(main_queue);
PHP_FUNCTION(queue_suspend);
PHP_FUNCTION(queue_resume);
PHP_FUNCTION(queue_set_specific);
PHP_FUNCTION(queue_get_specific);
PHP_FUNCTION(queue_set_error_handler);
PHP_FUNCTION(current_queue);
PHP_FUNCTION(queue_set_qos);
PHP_FUNCTION(queue_get_qos);
PHP_FUNCTION(queue_set_target);
PHP_FUNCTION(qos_class_self);

#endif