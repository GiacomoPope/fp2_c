#ifndef FP2_H
#define FP2_H

/* Angle brackets (not "fp.h") so the Modarith benchmark build can swap in
 * include/modarith/fp.h purely via -I order; a quoted include would always
 * resolve to this file's own directory first. */
#include <fp.h>

#define FP2_ENCODED_BYTES 2 * FP_ENCODED_BYTES

// Structure for representing elements in GF(p^2)
typedef struct fp2_t
{
    fp_t re, im;
} fp2_t;

void fp2_set_small(fp2_t *x, const int32_t val);
void fp2_set_one(fp2_t *x);
void fp2_set_zero(fp2_t *x);
void fp2_copy(fp2_t *x, const fp2_t *y);

/* Returns UINT32_MAX if this value is zero and 0 otherwise. */
uint32_t fp2_is_zero(const fp2_t *a);

/* Returns UINT32_MAX for equal values and 0 otherwise. */
uint32_t fp2_equals(const fp2_t *a, const fp2_t *b);

void fp2_add(fp2_t *x, const fp2_t *y, const fp2_t *z);
void fp2_sub(fp2_t *x, const fp2_t *y, const fp2_t *z);
void fp2_neg(fp2_t *x, const fp2_t *y);
void fp2_double(fp2_t *x, const fp2_t *y);
void fp2_half(fp2_t *x, const fp2_t *y);

void fp2_mul_small(fp2_t *x, const fp2_t *y, uint32_t n);
void fp2_mul(fp2_t *x, const fp2_t *y, const fp2_t *z);
void fp2_sqr(fp2_t *x, const fp2_t *y);

void fp2_mul_by_i(fp2_t *a, const fp2_t *b);
void fp2_frob(fp2_t *out, const fp2_t *in);
void fp2_inv(fp2_t *out, const fp2_t *in);

uint32_t fp2_is_square(const fp2_t *x);
void fp2_sqrt(fp2_t *a, const fp2_t *b);
void fp2_batched_inv(fp2_t *x, int len);

/* Always emits the unique canonical encoding. */
void fp2_encode(void *dst, const fp2_t *a);

/* Returns UINT32_MAX on success, 0 if the input is not canonical. */
uint32_t fp2_decode(fp2_t *d, const void *src);

/* Decodes arbitrary-length little-endian bytes, reducing x0 and x1 modulo p. */
void fp2_decode_reduce(fp2_t *d, const void *src, size_t len);

/* Constant-time helpers. ctl MUST be either 0 or UINT32_MAX. */
void fp2_select(fp2_t *r, const fp2_t *a, const fp2_t *b, uint32_t ctl);
void fp2_cond_swap(fp2_t *a, fp2_t *b, uint32_t ctl);
void fp2_cond_neg(fp2_t *a, uint32_t ctl);
uint32_t fp2_less_than(const fp2_t *a, const fp2_t *b);

#endif /* FP2_H */
