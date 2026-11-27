#ifndef GCD_SOURCE_H
#define GCD_SOURCE_H
#include "php_gcd.h"
void gcd_source_minit(void);
PHP_FUNCTION(source_create);
PHP_FUNCTION(source_set_event_handler);
PHP_FUNCTION(source_set_timer);
PHP_FUNCTION(source_resume);
PHP_FUNCTION(source_cancel);
#endif