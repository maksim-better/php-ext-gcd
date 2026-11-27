#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_gcd.h"

PHP_MINIT_FUNCTION(gcd)
{
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(gcd)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(gcd)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "gcd support", "enabled");
    php_info_print_table_row(2, "version", PHP_GCD_VERSION);
    php_info_print_table_row(2, "thread safety", "ZTS required");
    php_info_print_table_end();
}

static const zend_function_entry gcd_functions[] = {
    PHP_FE_END
};

zend_module_entry gcd_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_GCD_EXTNAME,
    gcd_functions,
    PHP_MINIT(gcd),
    PHP_MSHUTDOWN(gcd),
    NULL,
    NULL,
    PHP_MINFO(gcd),
    PHP_GCD_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_GCD
ZEND_GET_MODULE(gcd)
#endif