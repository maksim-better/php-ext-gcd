#include "copy.h"
#include <Zend/zend_execute.h>
#include <Zend/zend_hash.h>
#include <Zend/zend_map_ptr.h>
#include <Zend/zend_closures.h>

zend_op_array *gcd_get_closure_oa(zval *closure)
{
    if (Z_TYPE_P(closure) != IS_OBJECT
        || !instanceof_function(Z_OBJCE_P(closure), zend_ce_closure)) {
        return NULL;
    }
    zend_op_array *oa = (zend_op_array *) ((char *) Z_OBJ_P(closure) + sizeof(zend_object));
    if (oa->type != ZEND_USER_FUNCTION) {
        return NULL;   /* e.g. Closure::fromCallable('strlen') wraps an internal fn */
    }
    return oa;
}

static int gcd_array_copy(zval *dst, HashTable *src, int persistent);
void gcd_zval_free(zval *zv, int persistent);

static void gcd_persistent_dtor(zval *zv) { gcd_zval_free(zv, 1); }
static void gcd_request_dtor(zval *zv) { gcd_zval_free(zv, 0); }

int gcd_zval_copy(zval *dst, zval *src, int persistent)
{
    switch (Z_TYPE_P(src)) {
        case IS_NULL:
        case IS_FALSE:
        case IS_TRUE:
        case IS_LONG:
        case IS_DOUBLE:
            ZVAL_COPY(dst, src);
            return SUCCESS;
        case IS_STRING:
            ZVAL_STR(dst, zend_string_dup(Z_STR_P(src), persistent));
            return SUCCESS;
        case IS_ARRAY:
            return gcd_array_copy(dst, Z_ARRVAL_P(src), persistent);
        default:
            ZVAL_NULL(dst);
            return FAILURE;
    }
}

static int gcd_array_copy(zval *dst, HashTable *src, int persistent)
{
    HashTable *ht = pemalloc(sizeof(HashTable), persistent);
    zend_hash_init(ht, zend_hash_num_elements(src), NULL,
                   persistent ? gcd_persistent_dtor : gcd_request_dtor, persistent);
    ZVAL_ARR(dst, ht);

    zend_string *key;
    zend_ulong idx;
    zval *val;
    ZEND_HASH_FOREACH_KEY_VAL(src, idx, key, val) {
        zval copy;
        if (gcd_zval_copy(&copy, val, persistent) == FAILURE) {
            zend_hash_destroy(ht);
            pefree(ht, persistent);
            ZVAL_NULL(dst);
            return FAILURE;
        }
        if (key) {
            zend_hash_update(ht, zend_string_dup(key, persistent), &copy);
        } else {
            zend_hash_index_update(ht, idx, &copy);
        }
    } ZEND_HASH_FOREACH_END();
    return SUCCESS;
}

void gcd_zval_free(zval *zv, int persistent)
{
    switch (Z_TYPE_P(zv)) {
        case IS_STRING:
            zend_string_release(Z_STR_P(zv));
            break;
        case IS_ARRAY:
            zend_hash_destroy(Z_ARRVAL_P(zv));
            pefree(Z_ARRVAL_P(zv), persistent);
            break;
        default:
            break;
    }
    ZVAL_NULL(zv);
}

zend_op_array *gcd_op_array_copy_persistent(zend_op_array *src)
{
    if ((src->static_variables && zend_hash_num_elements(src->static_variables) > 0)
        || src->attributes) {
        return NULL;   /* use-capture / attributes: v1 reject */
    }
    zend_op_array *oa = pemalloc(sizeof(zend_op_array), 1);
    memcpy(oa, src, sizeof(zend_op_array));

    if (src->last) {
        /* opcodes + literals must stay contiguous: RT_CONSTANT addresses literals
         * relative to the opline. Preserve the source's opline->literals distance. */
        ptrdiff_t lit_offset = src->literals
            ? (char *) src->literals - (char *) src->opcodes
            : (ptrdiff_t) (sizeof(zend_op) * src->last);
        size_t block = (size_t) lit_offset + sizeof(zval) * src->last_literal;
        char *mem = pemalloc(block, 1);
        oa->opcodes = (zend_op *) mem;
        memcpy(oa->opcodes, src->opcodes, sizeof(zend_op) * src->last);
        oa->literals = src->last_literal ? (zval *)(mem + lit_offset) : NULL;
        for (uint32_t i = 0; i < src->last_literal; i++) {
            if (gcd_zval_copy_persistent(&oa->literals[i], &src->literals[i]) == FAILURE) {
                return NULL;
            }
        }
    }
    if (src->last_var) {
        oa->vars = pemalloc(sizeof(zend_string *) * src->last_var, 1);
        for (uint32_t i = 0; i < src->last_var; i++) {
            oa->vars[i] = src->vars[i] ? zend_string_dup(src->vars[i], 1) : NULL;
        }
    }
    if (src->arg_info && src->num_args) {
        oa->arg_info = pemalloc(sizeof(zend_arg_info) * src->num_args, 1);
        memcpy(oa->arg_info, src->arg_info, sizeof(zend_arg_info) * src->num_args);
        for (uint32_t i = 0; i < src->num_args; i++) {
            if (src->arg_info[i].name) {
                oa->arg_info[i].name = zend_string_dup(src->arg_info[i].name, 1);
            }
        }
    }
    oa->filename = src->filename ? zend_string_dup(src->filename, 1) : NULL;
    oa->function_name = src->function_name ? zend_string_dup(src->function_name, 1) : NULL;
    oa->doc_comment = src->doc_comment ? zend_string_dup(src->doc_comment, 1) : NULL;
    oa->run_time_cache__ptr = NULL;
    oa->refcount = NULL;
    oa->static_variables = NULL;
    oa->prop_info = NULL;
    oa->attributes = NULL;
    /* dynamic_func_defs (nested closures) kept as-is: valid while the parent
     * runtime lives (resident daemon). Deep-copy is a future hardening item. */
    if (src->last_try_catch) {
        oa->try_catch_array = pemalloc(sizeof(zend_try_catch_element) * src->last_try_catch, 1);
        memcpy(oa->try_catch_array, src->try_catch_array, sizeof(zend_try_catch_element) * src->last_try_catch);
    } else {
        oa->try_catch_array = NULL;
    }
    if (src->last_live_range) {
        oa->live_range = pemalloc(sizeof(zend_live_range) * src->last_live_range, 1);
        memcpy(oa->live_range, src->live_range, sizeof(zend_live_range) * src->last_live_range);
    } else {
        oa->live_range = NULL;
    }
    /* deep-copy nested closures/anon classes so concurrent workers don't race
     * on a shared op_array refcount (TSan-verified fix) */
    if (src->num_dynamic_func_defs && src->dynamic_func_defs) {
        oa->dynamic_func_defs = pemalloc(sizeof(zend_function *) * src->num_dynamic_func_defs, 1);
        oa->num_dynamic_func_defs = src->num_dynamic_func_defs;
        for (uint32_t i = 0; i < src->num_dynamic_func_defs; i++) {
            zend_function *f = src->dynamic_func_defs[i];
            if (f && f->type == ZEND_USER_FUNCTION) {
                zend_op_array *cp = gcd_op_array_copy_persistent(&f->op_array);
                zend_function *nf = pemalloc(sizeof(zend_function), 1);
                memset(nf, 0, sizeof(*nf));
                nf->type = ZEND_USER_FUNCTION;
                nf->op_array = *cp;
                pefree(cp, 1);
                oa->dynamic_func_defs[i] = nf;
            } else {
                oa->dynamic_func_defs[i] = f;
            }
        }
    }
    return oa;
}

static void gcd_op_array_free_contents(zend_op_array *oa)
{
    if (oa->literals) {
        for (uint32_t i = 0; i < oa->last_literal; i++) {
            gcd_zval_free(&oa->literals[i], 1);
        }
    }
    if (oa->opcodes) pefree(oa->opcodes, 1);  /* single block including literals */
    if (oa->live_range) pefree(oa->live_range, 1);
    if (oa->try_catch_array) pefree(oa->try_catch_array, 1);
    if (oa->vars) {
        for (uint32_t i = 0; i < oa->last_var; i++) {
            if (oa->vars[i]) zend_string_release(oa->vars[i]);
        }
        pefree(oa->vars, 1);
    }
    if (oa->arg_info) {
        for (uint32_t i = 0; i < oa->num_args; i++) {
            if (oa->arg_info[i].name) zend_string_release(oa->arg_info[i].name);
        }
        pefree(oa->arg_info, 1);
    }
    if (oa->filename) zend_string_release(oa->filename);
    if (oa->function_name) zend_string_release(oa->function_name);
    if (oa->doc_comment) zend_string_release(oa->doc_comment);
    if (oa->dynamic_func_defs) {
        for (uint32_t i = 0; i < oa->num_dynamic_func_defs; i++) {
            zend_function *f = oa->dynamic_func_defs[i];
            if (f && f->type == ZEND_USER_FUNCTION) {
                gcd_op_array_free_contents(&f->op_array);
                pefree(f, 1);
            }
        }
        pefree(oa->dynamic_func_defs, 1);
    }
}

void gcd_op_array_free_persistent(zend_op_array *oa)
{
    if (!oa) return;
    gcd_op_array_free_contents(oa);
    pefree(oa, 1);
}