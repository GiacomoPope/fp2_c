#include "fp2.h"

void
fp2_set_zero(fp2_t *x)
{
    fp_set_zero(&(x->re));
    fp_set_zero(&(x->im));
}

void
fp2_set_one(fp2_t *x)
{
    fp_set_one(&(x->re));
    fp_set_zero(&(x->im));
}

void
fp2_set_small(fp2_t *x, const int32_t val)
{
    fp_set_small(&(x->re), val);
    fp_set_zero(&(x->im));
}

uint32_t
fp2_is_zero(const fp2_t *a)
{
    return fp_is_zero(&(a->re)) & fp_is_zero(&(a->im));
}

uint32_t
fp2_equals(const fp2_t *a, const fp2_t *b)
{
    return fp_equals(&(a->re), &(b->re)) & fp_equals(&(a->im), &(b->im));
}

void
fp2_mul_small(fp2_t *x, const fp2_t *y, uint32_t n)
{
    fp_mul_small(&x->re, &y->re, n);
    fp_mul_small(&x->im, &y->im, n);
}

void
fp2_copy(fp2_t *x, const fp2_t *y)
{
    fp_copy(&(x->re), &(y->re));
    fp_copy(&(x->im), &(y->im));
}

void
fp2_add(fp2_t *x, const fp2_t *y, const fp2_t *z)
{
    fp_add(&(x->re), &(y->re), &(z->re));
    fp_add(&(x->im), &(y->im), &(z->im));
}

void
fp2_sub(fp2_t *x, const fp2_t *y, const fp2_t *z)
{
    fp_sub(&(x->re), &(y->re), &(z->re));
    fp_sub(&(x->im), &(y->im), &(z->im));
}

void
fp2_neg(fp2_t *x, const fp2_t *y)
{
    fp_neg(&(x->re), &(y->re));
    fp_neg(&(x->im), &(y->im));
}

void
fp2_double(fp2_t *x, const fp2_t *y)
{
    fp_double(&(x->re), &(y->re));
    fp_double(&(x->im), &(y->im));
}

void
fp2_half(fp2_t *x, const fp2_t *y)
{
    fp_half(&(x->re), &(y->re));
    fp_half(&(x->im), &(y->im));
}

void
fp2_mul(fp2_t *x, const fp2_t *y, const fp2_t *z)
{
    fp_difference_of_products(&x->re, &y->re, &z->re, &y->im, &z->im);
    fp_sum_of_products(&x->im, &y->re, &z->im, &y->im, &z->re);
}

void
fp2_sqr(fp2_t *x, const fp2_t *y)
{
    fp_t sum, diff;

    fp_add(&sum, &(y->re), &(y->im));
    fp_sub(&diff, &(y->re), &(y->im));
    fp_mul(&(x->im), &(y->re), &(y->im));
    fp_add(&(x->im), &(x->im), &(x->im));
    fp_mul(&(x->re), &sum, &diff);
}

void
fp2_mul_by_i(fp2_t *x, const fp2_t *y, uint32_t ctl)
{
    fp_t t0, t1;

    fp_neg(&t0, &(y->re));
    fp_neg(&t1, &(y->im));
    fp_select(&(x->re), &(y->im), &t1, -ctl);
    fp_select(&(x->im), &t0, &(y->re), -ctl);
}

void
fp2_frob(fp2_t *out, const fp2_t *in)
{
    fp_copy(&(out->re), &(in->re));
    fp_neg(&(out->im), &(in->im));
}

void
fp2_inv(fp2_t *x, const fp2_t *y)
{
    fp_t t0, t1;

    fp_sqr(&t0, &(y->re));
    fp_sqr(&t1, &(y->im));
    fp_add(&t0, &t0, &t1);
    fp_inv(&t0, &t0);
    fp_mul(&(x->re), &(y->re), &t0);
    fp_mul(&(x->im), &(y->im), &t0);
    fp_neg(&(x->im), &(x->im));
}

uint32_t
fp2_is_square(const fp2_t *x)
{
    fp_t t0, t1;

    fp_sqr(&t0, &(x->re));
    fp_sqr(&t1, &(x->im));
    fp_add(&t0, &t0, &t1);

    return fp_is_square(&t0);
}

void
fp2_sqrt(fp2_t *a, const fp2_t *b)
{
    fp_t x0, x1, t0, t1;

    /* From "Optimized One-Dimensional SQIsign Verification on Intel and Cortex-M4" by Aardal et al:
     * https://eprint.iacr.org/2024/1563 */

    // x0 = \delta = sqrt(a0^2 + a1^2).
    fp_sqr(&x0, &(b->re));
    fp_sqr(&x1, &(b->im));
    fp_add(&x0, &x0, &x1);
    fp_sqrt(&x0, &x0);

    // If a1 = 0, there is a risk of \delta = -a0, which makes x0 = 0 below.
    // In that case, we restore the value \delta = a0.
    fp_select(&x0, &x0, &(b->re), fp_is_zero(&(b->im)));
    // x0 = \delta + a0, t0 = 2 * x0.
    fp_add(&x0, &x0, &(b->re));
    fp_add(&t0, &x0, &x0);

    // x1 = t0^(p-3)/4
    fp_exp3div4(&x1, &t0);

    // x0 = x0 * x1, x1 = x1 * a1, t1 = (2x0)^2.
    fp_mul(&x0, &x0, &x1);
    fp_mul(&x1, &x1, &(a->im));
    fp_add(&t1, &x0, &x0);
    fp_sqr(&t1, &t1);
    // If t1 = t0, return x0 + x1*i, otherwise x1 - x0*i.
    fp_sub(&t0, &t0, &t1);
    uint32_t f = fp_is_zero(&t0);
    fp_neg(&t1, &x0);
    fp_copy(&t0, &x1);
    fp_select(&a->re, &t0, &x0, f);
    fp_select(&a->im, &t1, &x1, f);
}

void
fp2_batched_inv(fp2_t *x, int len)
{
    fp2_t t1[len], t2[len];
    fp2_t inverse;

    // x = x0,...,xn
    // t1 = x0, x0*x1, ... ,x0 * x1 * ... * xn
    fp2_copy(&t1[0], &x[0]);
    for (int i = 1; i < len; i++) {
        fp2_mul(&t1[i], &t1[i - 1], &x[i]);
    }

    // inverse = 1/ (x0 * x1 * ... * xn)
    fp2_copy(&inverse, &t1[len - 1]);
    fp2_inv(&inverse, &inverse);

    fp2_copy(&t2[0], &inverse);
    // t2 = 1/ (x0 * x1 * ... * xn), 1/ (x0 * x1 * ... * x(n-1)) , ... , 1/xO
    for (int i = 1; i < len; i++) {
        fp2_mul(&t2[i], &t2[i - 1], &x[len - i]);
    }

    fp2_copy(&x[0], &t2[len - 1]);

    for (int i = 1; i < len; i++) {
        fp2_mul(&x[i], &t1[i - 1], &t2[len - i - 1]);
    }
}

void
fp2_encode(void *dst, const fp2_t *a)
{
    uint8_t *buf = dst;
    fp_encode(buf, &(a->re));
    fp_encode(buf + FP_ENCODED_BYTES, &(a->im));
}

uint32_t
fp2_decode(fp2_t *d, const void *src)
{
    const uint8_t *buf = src;
    uint32_t re, im;

    re = fp_decode(&(d->re), buf);
    im = fp_decode(&(d->im), buf + FP_ENCODED_BYTES);
    return re & im;
}

void
fp2_decode_reduce(fp2_t *d, const void *src, size_t len)
{
    const uint8_t *buf = src;

    size_t r1 = len / 2;
    size_t r2 = len - r1;

    fp_decode_reduce(&(d->re), buf, r1);
    fp_decode_reduce(&(d->im), buf + FP_ENCODED_BYTES, r2);
}

void
fp2_select(fp2_t *d, const fp2_t *a0, const fp2_t *a1, uint32_t ctl)
{
    fp_select(&(d->re), &(a0->re), &(a1->re), ctl);
    fp_select(&(d->im), &(a0->im), &(a1->im), ctl);
}

void
fp2_cond_swap(fp2_t *a, fp2_t *b, uint32_t ctl)
{
    fp_cond_swap(&(a->re), &(b->re), ctl);
    fp_cond_swap(&(a->im), &(b->im), ctl);
}

// Returns UINT32_MAX if x1 < x2, 0 otherwise where x are represented as little endian integers of the form
// encode(x.re) || encode(x.im) where the real and imaginary components themselves are encoded as little endian
// integers.
// In other words, x1 < x2 if im(x1) < im(x2) or im(x1) == im(x2) and re(x1) < re(x2)
uint32_t
fp2_less_than(const fp2_t *x1, const fp2_t *x2)
{
    uint8_t buf1[FP2_ENCODED_BYTES];
    uint8_t buf2[FP2_ENCODED_BYTES];

    fp2_encode(buf1, x1);
    fp2_encode(buf2, x2);

    uint32_t result = 0;
    uint32_t all_equal_so_far = UINT32_MAX;

    for (size_t idx = FP2_ENCODED_BYTES; idx-- > 0;) {
        uint32_t less = ct_lt_u8(buf1[idx], buf2[idx]);
        uint32_t equal = ct_eq_u8(buf1[idx], buf2[idx]);
        result |= all_equal_so_far & less;
        all_equal_so_far &= equal;
    }

    return result;
}
