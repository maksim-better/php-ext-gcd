#ifndef GCD_QUEUE_H
#define GCD_QUEUE_H

#include "php_gcd.h"
#include <pthread.h>

struct _gcd_task;

typedef struct _gcd_queue {
    char *label;
    int width;                 /* 1 = serial, >1 = concurrent */
    pthread_mutex_t drain_lock;
    pthread_mutex_t list_mutex;
    struct _gcd_task *head, *tail;  /* pending FIFO list (serial drain) */
    int draining;
} gcd_queue;

typedef struct {
    gcd_queue *q;
    zend_object std;
} gcd_queue_object;

extern zend_class_entry *gcd_queue_ce;
extern zend_class_entry *gcd_capture_ce;

void gcd_queue_minit(void);
gcd_queue *gcd_queue_from_zval(zval *zv);
void gcd_queue_enqueue(gcd_queue *q, struct _gcd_task *t);
void gcd_queue_drain(gcd_queue *q);

PHP_FUNCTION(queue_create);
PHP_FUNCTION(global_queue);

#endif