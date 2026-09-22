#ifndef FP_H
#define FP_H

#include <stddef.h>
#include <stdint.h>

#include "fp_defs.h"

typedef struct {
    fp_limb_t limb[FP_LIMBS];
} fp_t;

/* Set fp to 0 */
void fp_set_zero(fp_t *x);

/* Set fp to 1 */
void fp_set_one(fp_t *x);

/* Copy b into a */
void fp_copy(fp_t *a, const fp_t *b);

/* Set fp to val (val may be negative; result is reduced mod p). */
void fp_set_small(fp_t *x, const int32_t val);

/* Returns UINT32_MAX for equal values and 0 otherwise. */
uint32_t fp_equals(const fp_t *a, const fp_t *b);

/* Returns UINT32_MAX if this value is zero and 0 otherwise. */
uint32_t fp_is_zero(const fp_t *a);

/* r = a + b in GF(p). Aliasing is allowed. */
void fp_add(fp_t *r, const fp_t *a, const fp_t *b);

/* r = a - b in GF(p). Aliasing is allowed. */
void fp_sub(fp_t *r, const fp_t *a, const fp_t *b);

/* r1 = a + b, r2 = a - b in GF(p), sharing a single read of a and b.
   Aliasing is allowed. */
void fp_hadamard(fp_t *r1, fp_t *r2, const fp_t *a, const fp_t *b);

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
void fp_pow_pubexp(fp_t *r, const fp_t *a, const fp_limb_t e[FP_LIMBS]);

/* Computes a square root. Returns UINT32_MAX on success, 0 otherwise. */
uint32_t fp_sqrt(fp_t *r, const fp_t *a);

/* Computes the inverse of a, if a = 0 then r is set to 0 */
void fp_inv(fp_t *r, const fp_t *a);

/* Computes r = a^((p + 3) / 4) */
void fp_exp3div4(fp_t *r, const fp_t *a);

/* Invert all elements in place with Montgomery's trick. If any element is zero, they all become zero. */
void fp_batch_invert(fp_t *x, size_t len);

/* 
 * Legendre symbol on this value. Return value is:
 *  0   if this value is zero
 * +1   if this value is a non-zero quadratic residue
 * -1   if this value is not a quadratic residue 
 */
int32_t fp_legendre(const fp_t *a);

/* Returns UINT32_MAX if a is square and zero otherwise */
uint32_t fp_is_square(const fp_t *x);

/* Returns UINT32_MAX on success, 0 if the input is not canonical. */
uint32_t fp_decode(fp_t *out, const uint8_t in[FP_ENCODED_BYTES]);

/* Decodes arbitrary-length little-endian bytes, reducing modulo p. */
void fp_decode_reduce(fp_t *out, const uint8_t *in, size_t len);

/* Always emits the unique canonical encoding. */
void fp_encode(uint8_t out[FP_ENCODED_BYTES], const fp_t *x);

// Returns UINT32_MAX if a < b, 0 otherwise
static inline uint32_t
ct_lt_u8(uint8_t a, uint8_t b)
{
    uint32_t borrow_bit = (((uint32_t)a - (uint32_t)b) >> 8) & 1u;
    return (uint32_t)(-(int32_t)borrow_bit);
}

// Returns UINT32_MAX if a == b, 0 otherwise
static inline uint32_t
ct_eq_u8(uint8_t a, uint8_t b)
{
    uint32_t x = (uint32_t)(a ^ b);
    uint32_t nonzero = (x | (uint32_t)(-(int32_t)x)) >> 31;
    return ~((uint32_t)(-(int32_t)nonzero));
}

// Returns UINT32_MAX if a < b, 0 otherwise where x are represented as little endian integers
uint32_t fp_less_than(const fp_t *x1, const fp_t *x2);

#endif /* FP_H */
