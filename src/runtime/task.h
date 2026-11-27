#ifndef GCD_TASK_H
#define GCD_TASK_H

#include "php_gcd.h"
#include <pthread.h>

typedef struct _gcd_task {
    zend_op_array *op_array;   /* persistent closure body */
    zval argv;                 /* persistent IS_ARRAY (or NULL) */
    uint32_t argc;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int done;
    int failed;
    struct _gcd_task *next;
} gcd_task;

gcd_task *gcd_task_create(zend_op_array *oa, zval *argv, uint32_t argc);
void gcd_task_execute(gcd_task *t);  /* runs inside a worker PHP context */
void gcd_task_wait(gcd_task *t);
void gcd_task_destroy(gcd_task *t);

#endif