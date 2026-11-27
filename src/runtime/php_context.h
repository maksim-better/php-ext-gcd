#ifndef GCD_PHP_CONTEXT_H
#define GCD_PHP_CONTEXT_H

#include "php_gcd.h"

/* Boot an isolated PHP runtime on the CURRENT thread (S1-proven mechanism). */
int gcd_context_boot(void);
/* Tear down the current thread's PHP runtime. */
void gcd_context_shutdown(void);
/* Capture parent (main thread) SAPI globals; call once from MINIT. */
void gcd_context_capture_parent(void);

#endif