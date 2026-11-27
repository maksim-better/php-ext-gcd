#ifndef GCD_ERROR_H
#define GCD_ERROR_H

#include "php_gcd.h"
#include "task.h"

void gcd_error_minit(void);
void gcd_error_set_global(zval *closure);
void gcd_error_report(gcd_task *t);

PHP_FUNCTION(set_error_handler);

#endif