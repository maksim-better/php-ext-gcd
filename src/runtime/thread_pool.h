#ifndef GCD_THREAD_POOL_H
#define GCD_THREAD_POOL_H

#include "task.h"

/* Lazily-initialized global worker pool. Each worker boots one PHP context. */
void gcd_pool_submit(gcd_task *t);
void gcd_pool_shutdown(void);

#endif