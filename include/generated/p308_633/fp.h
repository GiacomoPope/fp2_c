#ifndef FP_H
#define FP_H

#include <stddef.h>
#include <stdint.h>

#define FP_LIMBS 5
#define FP_BITS 318
#define FP_ENCODED_LENGTH 40
#define FP_DECODE_REDUCE_CHUNK 32

typedef struct {
    uint64_t limb[FP_LIMBS];
} fp_t;

/* Returns UINT32_MAX for equal values and 0 otherwise. */
uint32_t fp_equals(const fp_t *a, const fp_t *b);

/* Returns UINT32_MAX if this value is zero and 0 otherwise. */
uint32_t fp_is_zero(const fp_t *a);

/* r = a + b in GF(p). Aliasing is allowed. */
void fp_add(fp_t *r, const fp_t *a, const fp_t *b);

/* r = a - b in GF(p). Aliasing is allowed. */
void fp_sub(fp_t *r, const fp_t *a, const fp_t *b);

/* r = -a in GF(p). Aliasing is allowed. */
void fp_neg(fp_t *r, const fp_t *a);

/* r = 2*a in GF(p). Aliasing is allowed. */
void fp_double(fp_t *r, const fp_t *a);

/* r = a / 2 in GF(p). Aliasing is allowed. */
void fp_half(fp_t *r, const fp_t *a);

/* r = k*a in GF(p), for a signed 32-bit k. Aliasing is allowed. */
void fp_mul_small(fp_t *r, const fp_t *a, int32_t k);

/* r = a * b in GF(p). Aliasing is allowed. */
void fp_mul(fp_t *r, const fp_t *a, const fp_t *b);

/* r = a^2 in GF(p). Aliasing is allowed. */
void fp_sqr(fp_t *r, const fp_t *a);

/* r = a squared n times. */
void fp_n_sqr(fp_t *r, const fp_t *a, uint32_t n);

/* Computes r = a1*b1 + a2*b2 used for efficient fp2 arithmetic */
void fp_sum_of_products(fp_t *r, const fp_t *a1, const fp_t *b1,
                        const fp_t *a2, const fp_t *b2);
    
/* Computes r = a1*b1 - a2*b2 used for efficient fp2 arithmetic */
void fp_difference_of_products(fp_t *r, const fp_t *a1, const fp_t *b1,
                               const fp_t *a2, const fp_t *b2);

/* Constant-time helpers. ctl MUST be either 0 or UINT32_MAX. */
void fp_select(fp_t *r, const fp_t *a, const fp_t *b, uint32_t ctl);
void fp_cond_swap(fp_t *a, fp_t *b, uint32_t ctl);
void fp_cond_neg(fp_t *a, uint32_t ctl);

/* r = a^e for a public exponent encoded as FP_LIMBS little-endian words. */
void fp_pow_pubexp(fp_t *r, const fp_t *a, const uint64_t e[FP_LIMBS]);

/* Computes a square root. Returns UINT32_MAX on success, 0 otherwise. */
uint32_t fp_sqrt(fp_t *r, const fp_t *a);
void fp_inv(fp_t *r, const fp_t *a);

/* Invert all elements in place with Montgomery's trick. Zeros remain zero. */
void fp_batch_invert(fp_t *x, size_t len);

/* 
 * Legendre symbol on this value. Return value is:
 *  0   if this value is zero
 * +1   if this value is a non-zero quadratic residue
 * -1   if this value is not a quadratic residue 
 */
int32_t fp_legendre(const fp_t *a);

/* Returns UINT32_MAX on success, 0 if the input is not canonical. */
uint32_t fp_decode(fp_t *out, const uint8_t in[FP_ENCODED_LENGTH]);

/* Decodes arbitrary-length little-endian bytes, reducing modulo p. */
void fp_decode_reduce(fp_t *out, const uint8_t *in, size_t len);

/* Always emits the unique canonical encoding. */
void fp_encode(uint8_t out[FP_ENCODED_LENGTH], const fp_t *x);

#endif /* FP_H */
