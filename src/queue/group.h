#ifndef GCD_GROUP_H
#define GCD_GROUP_H

#include "php_gcd.h"

typedef struct _gcd_group gcd_group;

void gcd_group_minit(void);
void gcd_group_leave(gcd_group *g);
gcd_group *gcd_group_from_zval(zval *zv);

PHP_FUNCTION(group_create);
PHP_FUNCTION(group_enter);
PHP_FUNCTION(group_leave);
PHP_FUNCTION(group_wait);
PHP_FUNCTION(group_async);

#endif