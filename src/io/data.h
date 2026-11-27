#ifndef GCD_DATA_H
#define GCD_DATA_H
#include "php_gcd.h"
void gcd_data_minit(void);
PHP_FUNCTION(data_create);
PHP_FUNCTION(data_concat);
PHP_FUNCTION(data_subrange);
PHP_FUNCTION(data_create_map);
#endif