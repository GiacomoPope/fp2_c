#include "fp.h"
#include "fp2.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t rng_state = UINT64_C(0x123456789abcdef0);

static uint64_t
rng64(void)
{
    rng_state = rng_state * UINT64_C(6364136223846793005) +
                UINT64_C(1442695040888963407);
    return rng_state;
}

static void
random_bytes(uint8_t *out, size_t len)
{
    for (size_t i = 0; i < len; i += 8) {
        uint64_t w = rng64();
        size_t n = len - i;
        if (n > 8)
            n = 8;
        for (size_t j = 0; j < n; j++)
            out[i + j] = (uint8_t)(w >> (8 * j));
    }
}

static void
random_element(fp2_t *x)
{
    uint8_t bytes[FP2_ENCODED_BYTES + 32];
    random_bytes(bytes, sizeof(bytes));
    fp2_decode_reduce(x, bytes, sizeof(bytes));
}

static void
test_add(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, b, c, ab, abc, bc, a_bc, ba, zero, neg, sum;
        random_element(&a);
        random_element(&b);
        random_element(&c);

        fp2_add(&ab, &a, &b);
        fp2_add(&abc, &ab, &c);
        fp2_add(&bc, &b, &c);
        fp2_add(&a_bc, &a, &bc);
        assert(fp2_equals(&abc, &a_bc) == UINT32_MAX);

        fp2_add(&ba, &b, &a);
        assert(fp2_equals(&ab, &ba) == UINT32_MAX);

        fp2_set_zero(&zero);
        fp2_add(&sum, &a, &zero);
        assert(fp2_equals(&sum, &a) == UINT32_MAX);

        fp2_neg(&neg, &a);
        fp2_add(&sum, &a, &neg);
        assert(fp2_is_zero(&sum) == UINT32_MAX);
    }
}

static void
test_sub(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, b, c, ab, abc, bc, a_bc, ba, neg_ba, zero, diff;
        random_element(&a);
        random_element(&b);
        random_element(&c);

        fp2_sub(&ab, &a, &b);
        fp2_sub(&abc, &ab, &c);
        fp2_add(&bc, &b, &c);
        fp2_sub(&a_bc, &a, &bc);
        assert(fp2_equals(&abc, &a_bc) == UINT32_MAX);

        fp2_sub(&ba, &b, &a);
        fp2_neg(&neg_ba, &ba);
        assert(fp2_equals(&ab, &neg_ba) == UINT32_MAX);

        fp2_set_zero(&zero);
        fp2_sub(&diff, &a, &zero);
        assert(fp2_equals(&diff, &a) == UINT32_MAX);

        fp2_sub(&diff, &a, &a);
        assert(fp2_is_zero(&diff) == UINT32_MAX);
    }
}

static void
test_neg(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, neg, sum;
        random_element(&a);

        fp2_neg(&neg, &a);
        fp2_add(&sum, &a, &neg);
        assert(fp2_is_zero(&sum) == UINT32_MAX);
    }
}

static void
test_double(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, want, got;
        random_element(&a);

        fp2_add(&want, &a, &a);
        fp2_double(&got, &a);
        assert(fp2_equals(&got, &want) == UINT32_MAX);
    }
}

static void
test_half(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, h, twice;
        random_element(&a);

        fp2_half(&h, &a);
        fp2_double(&twice, &h);
        assert(fp2_equals(&twice, &a) == UINT32_MAX);
    }
}

static void
test_mul(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, b, c, ab, abc, bc, a_bc;
        fp2_t bpc, a_bpc, ab2, ac, sum, ba, one, zero, d;
        random_element(&a);
        random_element(&b);
        random_element(&c);

        fp2_mul(&ab, &a, &b);
        fp2_mul(&abc, &ab, &c);
        fp2_mul(&bc, &b, &c);
        fp2_mul(&a_bc, &a, &bc);
        assert(fp2_equals(&abc, &a_bc) == UINT32_MAX);

        fp2_add(&bpc, &b, &c);
        fp2_mul(&a_bpc, &a, &bpc);
        fp2_mul(&ab2, &a, &b);
        fp2_mul(&ac, &a, &c);
        fp2_add(&sum, &ab2, &ac);
        assert(fp2_equals(&a_bpc, &sum) == UINT32_MAX);

        fp2_mul(&ba, &b, &a);
        assert(fp2_equals(&ab, &ba) == UINT32_MAX);

        fp2_set_one(&one);
        fp2_mul(&d, &a, &one);
        assert(fp2_equals(&d, &a) == UINT32_MAX);

        fp2_set_zero(&zero);
        fp2_mul(&d, &a, &zero);
        assert(fp2_is_zero(&d) == UINT32_MAX);

        /* Aliasing the output with either input must give the same result
           as a fresh destination. */
        fp2_t want, got;
        fp2_mul(&want, &a, &b);
        got = a;
        fp2_mul(&got, &got, &b);
        assert(fp2_equals(&got, &want) == UINT32_MAX);
        got = b;
        fp2_mul(&got, &a, &got);
        assert(fp2_equals(&got, &want) == UINT32_MAX);
    }
}

static void
test_mul_by_i(void)
{
    for (unsigned n = 0; n < 1000; n++) {
        fp2_t a, i, want, got;
        random_element(&a);

        /* i = 0 + 1*i, so a*i via fp2_mul is the reference. */
        fp2_set_zero(&i);
        fp_set_small(&i.im, 1);
        fp2_mul(&want, &a, &i);
        fp2_mul_by_i(&got, &a);
        assert(fp2_equals(&got, &want) == UINT32_MAX);

        /* Aliasing the output with the input must give the same result. */
        got = a;
        fp2_mul_by_i(&got, &got);
        assert(fp2_equals(&got, &want) == UINT32_MAX);

        /* i^4 == 1. */
        fp2_t x = a;
        for (int k = 0; k < 4; k++)
            fp2_mul_by_i(&x, &x);
        assert(fp2_equals(&x, &a) == UINT32_MAX);
    }
}

static void
test_mul_small(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, got, c, want;
        random_element(&a);
        uint32_t k = (uint32_t)rng64();

        fp2_mul_small(&got, &a, k);
        fp2_set_small(&c, k);
        fp2_mul(&want, &a, &c);
        assert(fp2_equals(&got, &want) == UINT32_MAX);
    }
}

static void
test_sqr(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, sq, aa, zero, dz;
        random_element(&a);

        fp2_sqr(&sq, &a);
        fp2_mul(&aa, &a, &a);
        assert(fp2_equals(&sq, &aa) == UINT32_MAX);

        fp2_set_zero(&zero);
        fp2_sqr(&dz, &zero);
        assert(fp2_is_zero(&dz) == UINT32_MAX);
    }
}

static void
test_inv(void)
{
    fp2_t zero;
    fp2_set_zero(&zero);
    fp2_t invz;
    fp2_inv(&invz, &zero);
    assert(fp2_equals(&invz, &zero) == UINT32_MAX);

    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, inv, check, one;
        random_element(&a);

        fp2_inv(&inv, &a);
        fp2_mul(&check, &inv, &a);
        fp2_set_one(&one);
        assert(fp2_equals(&check, &one) == UINT32_MAX);
    }
}

static void
test_ct_helpers(void)
{
    fp2_t a, b, r;
    random_element(&a);
    random_element(&b);

    fp2_select(&r, &a, &b, 0);
    assert(fp2_equals(&r, &a) == UINT32_MAX);
    fp2_select(&r, &a, &b, UINT32_MAX);
    assert(fp2_equals(&r, &b) == UINT32_MAX);

    fp2_t expected;
    fp2_neg(&expected, &b);
    fp2_cond_neg(&r, 0);
    assert(fp2_equals(&r, &b) == UINT32_MAX);
    fp2_cond_neg(&r, UINT32_MAX);
    assert(fp2_equals(&r, &expected) == UINT32_MAX);

    fp2_t a0 = a, b0 = b;
    fp2_cond_swap(&a, &b, 0);
    assert(fp2_equals(&a, &a0) == UINT32_MAX);
    assert(fp2_equals(&b, &b0) == UINT32_MAX);
    fp2_cond_swap(&a, &b, UINT32_MAX);
    assert(fp2_equals(&a, &b0) == UINT32_MAX);
    assert(fp2_equals(&b, &a0) == UINT32_MAX);
}

static void
test_roundtrip(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, b;
        uint8_t enc[FP2_ENCODED_BYTES];
        random_element(&a);

        fp2_encode(enc, &a);
        assert(fp2_decode(&b, enc) == UINT32_MAX);
        assert(fp2_equals(&a, &b) == UINT32_MAX);
    }
}

static void
test_batched_inv(void)
{
    for (int len = 0; len <= 257; len += 17) {
        fp2_t *v = len ? malloc((size_t)len * sizeof(*v)) : NULL;
        fp2_t *want = len ? malloc((size_t)len * sizeof(*want)) : NULL;
        assert(len == 0 || (v != NULL && want != NULL));

        for (int i = 0; i < len; i++) {
            random_element(&v[i]);
            if ((i % 11) == 0)
                fp2_set_zero(&v[i]);
            want[i] = v[i];
        }

        fp2_batched_inv(v, len);
        for (int i = 0; i < len; i++) {
            if (fp2_is_zero(&want[i]) == UINT32_MAX) {
                assert(fp2_is_zero(&v[i]) == UINT32_MAX);
            } else {
                fp2_t check, one;
                fp2_mul(&check, &v[i], &want[i]);
                fp2_set_one(&one);
                assert(fp2_equals(&check, &one) == UINT32_MAX);
            }
        }
        free(v);
        free(want);
    }
}

static void
test_sqrt(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp2_t a, aa, root, neg;
        random_element(&a);

        fp2_sqr(&aa, &a);
        assert(fp2_is_square(&aa) == UINT32_MAX);

        fp2_sqrt(&root, &aa);
        fp2_neg(&neg, &root);
        assert(fp2_equals(&a, &root) == UINT32_MAX || fp2_equals(&a, &neg) == UINT32_MAX);
    }
}

int
main(void)
{
    test_roundtrip();
    test_add();
    test_sub();
    test_neg();
    test_double();
    test_half();
    test_mul();
    test_mul_by_i();
    test_mul_small();
    test_sqr();
    test_ct_helpers();
    test_inv();
    test_batched_inv();
    test_sqrt();

    puts("all tests passed");
    return 0;
}
