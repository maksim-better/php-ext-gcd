#ifndef GCD_DISPATCH_H
#define GCD_DISPATCH_H

#include "php_gcd.h"

PHP_FUNCTION(sync);
PHP_FUNCTION(async);
PHP_FUNCTION(barrier_sync);
PHP_FUNCTION(barrier_async);
PHP_FUNCTION(apply);

#endif