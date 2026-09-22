#ifndef FP_H
#define FP_H

// WARNING: this file is a lazy wrapper just to get benchmarks working
// and some functions have been written with secret dependant branching
// DO NOT USE for production code.

#include "fp_scott.c"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    spint limb[Nlimbs];
} fp_t;

#define FP_ENCODED_BYTES Nbytes

static inline void
fp_set_zero(fp_t *x)
{
    modzer(x->limb);
}

static inline void
fp_set_one(fp_t *x)
{
    modone(x->limb);
}

static inline void
fp_copy(fp_t *a, const fp_t *b)
{
    modcpy(b->limb, a->limb);
}

static inline void
fp_set_small(fp_t *x, const int32_t val)
{
    if (val < 0) {
        modint(-val, x->limb);
        modneg(x->limb, x->limb);
    } else {
        modint(val, x->limb);
    }
}

static inline uint32_t
fp_equals(const fp_t *a, const fp_t *b)
{
    return modcmp(a->limb, b->limb) ? UINT32_MAX : 0;
}

static inline uint32_t
fp_is_zero(const fp_t *a)
{
    return modis0(a->limb) ? UINT32_MAX : 0;
}

static inline void
fp_add(fp_t *r, const fp_t *a, const fp_t *b)
{
    modadd(a->limb, b->limb, r->limb);
}

static inline void
fp_sub(fp_t *r, const fp_t *a, const fp_t *b)
{
    modsub(a->limb, b->limb, r->limb);
}

static inline void
fp_neg(fp_t *r, const fp_t *a)
{
    modneg(a->limb, r->limb);
}

static inline void
fp_hadamard(fp_t *r1, fp_t *r2, const fp_t *a, const fp_t *b)
{
    fp_t sum, diff;
    modadd(a->limb, b->limb, sum.limb);
    modsub(a->limb, b->limb, diff.limb);
    *r1 = sum;
    *r2 = diff;
}

static inline void
fp_double(fp_t *r, const fp_t *a)
{
    modadd(a->limb, a->limb, r->limb);
}

static inline void
fp_half(fp_t *r, const fp_t *a)
{
    /* modhaf halves in place, so copy first to support r != a. */
    modcpy(a->limb, r->limb);
    modhaf(r->limb);
}

static inline void
fp_mul_small(fp_t *r, const fp_t *a, int32_t k)
{
    if (k < 0) {
        modmli(a->limb, -k, r->limb);
        modneg(r->limb, r->limb);
    } else {
        modmli(a->limb, k, r->limb);
    }
}

static inline void
fp_mul(fp_t *r, const fp_t *a, const fp_t *b)
{
    modmul(a->limb, b->limb, r->limb);
}

static inline void
fp_sqr(fp_t *r, const fp_t *a)
{
    modsqr(a->limb, r->limb);
}

static inline void
fp_sum_of_products(fp_t *r, const fp_t *a1, const fp_t *b1,
                    const fp_t *a2, const fp_t *b2)
{
    spint t1[Nlimbs], t2[Nlimbs];

    modmul(a1->limb, b1->limb, t1);
    modmul(a2->limb, b2->limb, t2);
    modadd(t1, t2, r->limb);
}

static inline void
fp_difference_of_products(fp_t *r, const fp_t *a1, const fp_t *b1,
                           const fp_t *a2, const fp_t *b2)
{
    spint t1[Nlimbs], t2[Nlimbs];

    modmul(a1->limb, b1->limb, t1);
    modmul(a2->limb, b2->limb, t2);
    modsub(t1, t2, r->limb);
}

static inline void
fp_select(fp_t *r, const fp_t *a, const fp_t *b, uint32_t ctl)
{
    modcpy(a->limb, r->limb);
    modcmv(ctl != 0, b->limb, r->limb);
}

static inline void
fp_cond_swap(fp_t *a, fp_t *b, uint32_t ctl)
{
    modcsw(ctl != 0, a->limb, b->limb);
}

static inline void
fp_cond_neg(fp_t *a, uint32_t ctl)
{
    fp_t neg;
    fp_neg(&neg, a);
    fp_select(a, a, &neg, ctl);
}

static inline void
fp_exp3div4(fp_t *r, const fp_t *a)
{
    modpro(a->limb, r->limb);
}

static inline void
fp_inv(fp_t *r, const fp_t *a)
{
    modinv(a->limb, NULL, r->limb);
}

static inline uint32_t
fp_is_square(const fp_t *x)
{
    if (modis0(x->limb))
        return 0;
    return modqr(NULL, x->limb) ? UINT32_MAX : 0;
}

static inline int32_t
fp_legendre(const fp_t *x)
{
    if (modis0(x->limb))
        return 0;
    return modqr(NULL, x->limb) ? 1 : -1;
}

static inline uint32_t
fp_sqrt(fp_t *r, const fp_t *a)
{
    spint h[Nlimbs];

    modpro(a->limb, h);
    modsqrt(a->limb, h, r->limb);
    return modqr(h, a->limb) ? UINT32_MAX : 0;
}

static inline void
fp_decode_reduce(fp_t *out, const uint8_t *in, size_t len)
{
    spint acc[Nlimbs], radix[Nlimbs], digit[Nlimbs];

    modzer(acc);
    modint(256, radix);

    for (size_t i = len; i-- > 0;) {
        modmul(acc, radix, acc);
        modint((int)in[i], digit);
        modadd(acc, digit, acc);
    }

    modcpy(acc, out->limb);
}

static inline void
fp_encode(uint8_t out[FP_ENCODED_BYTES], const fp_t *x)
{
    char buf[Nbytes];

    modexp(x->limb, buf);
    for (size_t i = 0; i < Nbytes; i++)
        out[i] = (uint8_t)buf[Nbytes - 1 - i];
}

static inline uint32_t
fp_decode(fp_t *out, const uint8_t in[FP_ENCODED_BYTES])
{
    char buf[Nbytes];

    for (size_t i = 0; i < Nbytes; i++)
        buf[i] = (char)in[Nbytes - 1 - i];

    return modimp(buf, out->limb) ? UINT32_MAX : 0;
}

static inline uint32_t
ct_lt_u8(uint8_t a, uint8_t b)
{
    uint32_t borrow_bit = (((uint32_t)a - (uint32_t)b) >> 8) & 1u;
    return (uint32_t)(-(int32_t)borrow_bit);
}

static inline uint32_t
ct_eq_u8(uint8_t a, uint8_t b)
{
    uint32_t x = (uint32_t)(a ^ b);
    uint32_t nonzero = (x | (uint32_t)(-(int32_t)x)) >> 31;
    return ~((uint32_t)(-(int32_t)nonzero));
}

#endif /* FP_H */
