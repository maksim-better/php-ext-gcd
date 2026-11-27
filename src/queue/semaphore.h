#ifndef GCD_SEMAPHORE_H
#define GCD_SEMAPHORE_H
#include "php_gcd.h"
void gcd_semaphore_minit(void);
PHP_FUNCTION(semaphore_create);
PHP_FUNCTION(semaphore_open);
PHP_FUNCTION(semaphore_wait);
PHP_FUNCTION(semaphore_signal);
#endif