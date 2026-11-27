#ifndef PHP_GCD_H
#define PHP_GCD_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

/* ext-gcd 硬约束：必须 ZTS（线程安全）PHP 构建。NTS 构建在编译期拒绝。 */
#ifndef ZTS
#error "ext-gcd requires a ZTS (thread-safe) PHP build. Rebuild PHP with --enable-zts."
#endif

#define PHP_GCD_VERSION "0.1.0"
#define PHP_GCD_EXTNAME "gcd"

extern zend_module_entry gcd_module_entry;
#define phpext_gcd_ptr &gcd_module_entry

PHP_MINIT_FUNCTION(gcd);
PHP_MSHUTDOWN_FUNCTION(gcd);
PHP_MINFO_FUNCTION(gcd);

#endif /* PHP_GCD_H */