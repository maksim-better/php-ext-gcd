#ifndef GCD_TASK_H
#define GCD_TASK_H

#include "php_gcd.h"
#include <pthread.h>

typedef struct _gcd_task {
    zend_op_array *op_array;   /* persistent closure body */
    zval argv;                 /* persistent IS_ARRAY (or NULL) */
    uint32_t argc;
    struct _gcd_queue *queue;  /* owning queue (serial drain) */
    struct _gcd_group *group;  /* optional group (auto leave) */
    int is_sync;               /* 1: submitter waits+frees; 0: worker frees */
    int is_barrier;            /* exclusive drain barrier */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int done;
    int failed;
    struct _gcd_task *next;
} gcd_task;

gcd_task *gcd_task_create(zend_op_array *oa, zval *argv, uint32_t argc);
gcd_task *gcd_task_build(zval *closure, zval *argv);  /* from PHP Closure + argv */
int gcd_invoke(zend_op_array *oa, zval *argv, uint32_t argc);  /* run in current ctx */
void gcd_task_execute(gcd_task *t);  /* runs inside a worker PHP context */
void gcd_task_wait(gcd_task *t);
void gcd_task_destroy(gcd_task *t);

#endif