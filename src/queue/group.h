#ifndef GCD_GROUP_H
#define GCD_GROUP_H

#include "php_gcd.h"

typedef struct _gcd_group gcd_group;

void gcd_group_minit(void);
void gcd_group_leave(gcd_group *g);
gcd_group *gcd_group_new(void);
void gcd_group_free(gcd_group *g);
void gcd_group_enter(gcd_group *g);
void gcd_group_wait(gcd_group *g);
void gcd_group_retain(gcd_group *g);
void gcd_group_release(gcd_group *g);
gcd_group *gcd_group_from_zval(zval *zv);

PHP_FUNCTION(group_create);
PHP_FUNCTION(group_enter);
PHP_FUNCTION(group_leave);
PHP_FUNCTION(group_wait);
PHP_FUNCTION(group_async);
PHP_FUNCTION(group_notify);

#endif