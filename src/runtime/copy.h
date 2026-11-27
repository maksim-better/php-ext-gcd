#ifndef GCD_COPY_H
#define GCD_COPY_H

#include "php_gcd.h"
#include <Zend/zend_closures.h>

/* Deep-copy a zval. persistent=1 -> pemalloc storage (cross-thread);
 * persistent=0 -> request-local emalloc. Resources/objects rejected. */
int gcd_zval_copy(zval *dst, zval *src, int persistent);
void gcd_zval_free(zval *zv, int persistent);

static inline int gcd_zval_copy_persistent(zval *dst, zval *src) {
    return gcd_zval_copy(dst, src, 1);
}

/* Copy a closure's op_array into persistent storage. Returns NULL on reject. */
zend_op_array *gcd_op_array_copy_persistent(zend_op_array *src);
void gcd_op_array_free_persistent(zend_op_array *oa);

#endif