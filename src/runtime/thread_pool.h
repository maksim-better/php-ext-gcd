#ifndef GCD_THREAD_POOL_H
#define GCD_THREAD_POOL_H

#include "task.h"

/* Lazily-initialized global worker pool. Each worker boots one PHP context. */
void gcd_pool_submit(gcd_task *t);
void gcd_pool_shutdown(void);
void gcd_pool_pending_inc(void);
void gcd_pool_task_done(void);
void gcd_pool_drain(double timeout);
void gcd_pool_block_source_signal(int signo);  /* add to process-wide source mask, wake workers */
void gcd_pool_apply_source_mask(void);         /* block current source signals in calling thread */

#endif