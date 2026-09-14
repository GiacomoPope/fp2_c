#include "fp.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
random_element(fp_t *x)
{
    uint8_t bytes[FP_ENCODED_BYTES + 16];
    random_bytes(bytes, sizeof(bytes));
    fp_decode_reduce(x, bytes, sizeof(bytes));
}

static void
test_roundtrip(void)
{
    uint8_t in[FP_ENCODED_BYTES] = { 0 };
    uint8_t out[FP_ENCODED_BYTES];
    fp_t x, y;

    assert(fp_decode(&x, in) == UINT32_MAX);
    fp_encode(out, &x);
    assert(memcmp(in, out, sizeof(in)) == 0);

    in[0] = 1;
    assert(fp_decode(&x, in) == UINT32_MAX);
    fp_encode(out, &x);
    assert(memcmp(in, out, sizeof(in)) == 0);

    assert(fp_decode(&y, out) == UINT32_MAX);
    assert(fp_equals(&x, &y) == UINT32_MAX);
}

static void
test_equals(void)
{
    fp_t a, b;
    uint8_t zero[FP_ENCODED_BYTES] = { 0 };
    uint8_t one[FP_ENCODED_BYTES] = { 1 };

    assert(fp_decode(&a, zero) == UINT32_MAX);
    assert(fp_decode(&b, zero) == UINT32_MAX);
    assert(fp_equals(&a, &b) == UINT32_MAX);

    assert(fp_decode(&b, one) == UINT32_MAX);
    assert(fp_equals(&a, &b) == 0);
}

static void
test_is_zero(void)
{
    fp_t z = { { 0 } };
    fp_t x;

    assert(fp_is_zero(&z) == UINT32_MAX);

    x = z;
    x.limb[0] = 1;
    assert(fp_is_zero(&x) == 0);

    x = z;
    x.limb[FP_LIMBS - 1] = 1;
    assert(fp_is_zero(&x) == 0);
}

static void
test_small_mul(void)
{
    uint8_t two[FP_ENCODED_BYTES] = { 2 };
    uint8_t three[FP_ENCODED_BYTES] = { 3 };
    uint8_t six[FP_ENCODED_BYTES] = { 6 };
    fp_t a, b, c, want;
    assert(fp_decode(&a, two) == UINT32_MAX);
    assert(fp_decode(&b, three) == UINT32_MAX);
    assert(fp_decode(&want, six) == UINT32_MAX);
    fp_mul(&c, &a, &b);
    assert(fp_equals(&c, &want) == UINT32_MAX);
}

static void
test_random_add(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, b, ab, ba;
        random_element(&a);
        random_element(&b);

        fp_add(&ab, &a, &b);
        fp_add(&ba, &b, &a);
        assert(fp_equals(&ab, &ba) == UINT32_MAX);
    }
}

static void
test_random_mul(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, b, ab, ba;
        random_element(&a);
        random_element(&b);

        fp_mul(&ab, &a, &b);
        fp_mul(&ba, &b, &a);
        assert(fp_equals(&ab, &ba) == UINT32_MAX);
    }
}

static void
test_random_sub(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, b, r, check;
        random_element(&a);
        random_element(&b);

        fp_sub(&r, &a, &b);
        fp_add(&check, &r, &b);
        assert(fp_equals(&check, &a) == UINT32_MAX);
    }
}

static void
test_random_neg(void)
{
    fp_t zero;
    uint8_t z[FP_ENCODED_BYTES] = { 0 };
    assert(fp_decode(&zero, z) == UINT32_MAX);

    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, neg, check;
        random_element(&a);

        fp_neg(&neg, &a);
        fp_add(&check, &a, &neg);
        assert(fp_equals(&check, &zero) == UINT32_MAX);
    }
}

static void
test_random_sqrt(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, aa, root, neg, check;
        random_element(&a);

        fp_sqr(&aa, &a);
        assert(fp_sqrt(&root, &aa) == UINT32_MAX);

        fp_neg(&neg, &a);
        fp_sqr(&check, &root);
        assert(fp_equals(&check, &aa) == UINT32_MAX);
        assert(fp_equals(&root, &a) == UINT32_MAX ||
               fp_equals(&root, &neg) == UINT32_MAX);
    }
}

static void
test_small_inv(void)
{
    uint8_t two[FP_ENCODED_BYTES] = { 2 };
    uint8_t one[FP_ENCODED_BYTES] = { 1 };
    fp_t a, inv, check, want;
    assert(fp_decode(&a, two) == UINT32_MAX);
    assert(fp_decode(&want, one) == UINT32_MAX);
    fp_inv(&inv, &a);
    fp_mul(&check, &inv, &a);
    assert(fp_equals(&check, &want) == UINT32_MAX);
}

static void
test_random_inv(void)
{
    uint8_t zero[FP_ENCODED_BYTES] = { 0 };
    fp_t z, invz;
    assert(fp_decode(&z, zero) == UINT32_MAX);
    fp_inv(&invz, &z);
    assert(fp_equals(&invz, &z) == UINT32_MAX);

    for (unsigned i = 0; i < 5000; i++) {
        fp_t a, b, invb, c, check;
        random_element(&a);
        do {
            random_element(&b);
        } while (fp_equals(&b, &z) == UINT32_MAX);

        fp_inv(&invb, &b);
        fp_mul(&c, &a, &invb);
        fp_mul(&check, &c, &b);
        assert(fp_equals(&check, &a) == UINT32_MAX);
    }
}

static void
test_legendre(void)
{
    uint8_t zero[FP_ENCODED_BYTES] = { 0 };
    uint8_t one[FP_ENCODED_BYTES] = { 1 };
    fp_t z, o, nqr;
    assert(fp_decode(&z, zero) == UINT32_MAX);
    assert(fp_decode(&o, one) == UINT32_MAX);
    
    random_element(&nqr);
    fp_sqr(&nqr, &nqr);
    fp_neg(&nqr, &nqr);

    assert(fp_legendre(&z) == 0);
    assert(fp_legendre(&o) == 1);
    assert(fp_legendre(&nqr) == -1);

    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, aa;
        random_element(&a);
        fp_sqr(&aa, &a);
        assert(fp_legendre(&aa) == 1 || fp_equals(&aa, &z) == UINT32_MAX);
    }
}

static void
test_double(void)
{
    uint8_t two[FP_ENCODED_BYTES] = { 2 };
    fp_t x, got, want;
    assert(fp_decode(&x, two) == UINT32_MAX);

    fp_add(&want, &x, &x);
    fp_double(&got, &x);
    assert(fp_equals(&got, &want) == UINT32_MAX);

    for (unsigned i = 0; i < 1000; i++) {
        random_element(&x);
        fp_add(&want, &x, &x);
        fp_double(&got, &x);
        assert(fp_equals(&got, &want) == UINT32_MAX);
    }
}

static void
test_mul_small(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp_t x, got, want, tmp;
        random_element(&x);

        for (int32_t k = -32; k <= 32; k++) {
            fp_mul_small(&got, &x, k);

            if (k == 0) {
                uint8_t zero[FP_ENCODED_BYTES] = { 0 };
                fp_decode(&want, zero);
            } else {
                int32_t ak = k < 0 ? -k : k;
                want = x;
                for (int32_t j = 1; j < ak; j++)
                    fp_add(&want, &want, &x);
                if (k < 0)
                    fp_neg(&want, &want);
            }
            assert(fp_equals(&got, &want) == UINT32_MAX);
        }

        /* Exercise INT32_MIN without a 2^31-iteration reference loop. */
        tmp = x;
        for (unsigned j = 0; j < 31; j++)
            fp_double(&tmp, &tmp);
        fp_neg(&tmp, &tmp);
        fp_mul_small(&got, &x, INT32_MIN);
        assert(fp_equals(&got, &tmp) == UINT32_MAX);
    }
}

static void
test_ct_helpers(void)
{
    uint8_t one[FP_ENCODED_BYTES] = { 1 };
    uint8_t two[FP_ENCODED_BYTES] = { 2 };
    fp_t a, b, r;
    assert(fp_decode(&a, one) == UINT32_MAX);
    assert(fp_decode(&b, two) == UINT32_MAX);

    fp_select(&r, &a, &b, 0);
    assert(fp_equals(&r, &a) == UINT32_MAX);
    fp_select(&r, &a, &b, UINT32_MAX);
    assert(fp_equals(&r, &b) == UINT32_MAX);

    fp_cond_neg(&r, 0);
    assert(fp_equals(&r, &b) == UINT32_MAX);
    {
        fp_t expected;
        fp_neg(&expected, &b);
        fp_cond_neg(&r, UINT32_MAX);
        assert(fp_equals(&r, &expected) == UINT32_MAX);
    }

    fp_cond_swap(&a, &b, 0);
    {
        fp_t one2, two2;
        fp_decode(&one2, one);
        fp_decode(&two2, two);
        assert(fp_equals(&a, &one2) == UINT32_MAX);
        assert(fp_equals(&b, &two2) == UINT32_MAX);
    }
    fp_cond_swap(&a, &b, UINT32_MAX);
    {
        fp_t one2, two2;
        fp_decode(&one2, one);
        fp_decode(&two2, two);
        assert(fp_equals(&a, &two2) == UINT32_MAX);
        assert(fp_equals(&b, &one2) == UINT32_MAX);
    }
}


static void
test_half(void)
{
    for (unsigned i = 0; i < 2000; i++) {
        fp_t a, h, twice;
        random_element(&a);
        fp_half(&h, &a);
        fp_double(&twice, &h);
        assert(fp_equals(&twice, &a) == UINT32_MAX);
    }
}

static void
test_n_sqr(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, got, want;
        random_element(&a);
        fp_n_sqr(&got, &a, 0);
        assert(fp_equals(&got, &a) == UINT32_MAX);
        want = a;
        for (unsigned n = 0; n < 12; n++) {
            fp_n_sqr(&got, &a, n);
            assert(fp_equals(&got, &want) == UINT32_MAX);
            fp_sqr(&want, &want);
        }
    }
}

static void
test_sum_difference_of_products(void)
{
    for (unsigned i = 0; i < 2000; i++) {
        fp_t a1, b1, a2, b2;
        fp_t p1, p2, want_sum, want_diff, got;
        random_element(&a1);
        random_element(&b1);
        random_element(&a2);
        random_element(&b2);

        fp_mul(&p1, &a1, &b1);
        fp_mul(&p2, &a2, &b2);
        fp_add(&want_sum, &p1, &p2);
        fp_sub(&want_diff, &p1, &p2);

        fp_sum_of_products(&got, &a1, &b1, &a2, &b2);
        assert(fp_equals(&got, &want_sum) == UINT32_MAX);
        fp_difference_of_products(&got, &a1, &b1, &a2, &b2);
        assert(fp_equals(&got, &want_diff) == UINT32_MAX);
    }
}

static void
test_batch_invert(void)
{
    for (size_t len = 0; len <= 257; len += 17) {
        fp_t *v = len ? malloc(len * sizeof(*v)) : NULL;
        fp_t *want = len ? malloc(len * sizeof(*want)) : NULL;
        assert(len == 0 || (v != NULL && want != NULL));

        for (size_t i = 0; i < len; i++) {
            random_element(&v[i]);
            if ((i % 11) == 0) {
                uint8_t z[FP_ENCODED_BYTES] = { 0 };
                assert(fp_decode(&v[i], z) == UINT32_MAX);
            }
            want[i] = v[i];
        }

        fp_batch_invert(v, len);
        for (size_t i = 0; i < len; i++) {
            fp_t zero = { { 0 } };
            if (fp_equals(&want[i], &zero) == UINT32_MAX) {
                assert(fp_equals(&v[i], &zero) == UINT32_MAX);
            } else {
                fp_t check;
                fp_mul(&check, &v[i], &want[i]);
                fp_t one;
                uint8_t one_bytes[FP_ENCODED_BYTES] = { 0 };
                one_bytes[0] = 1;
                assert(fp_decode(&one, one_bytes) == UINT32_MAX);
                assert(fp_equals(&check, &one) == UINT32_MAX);
            }
        }
        free(v);
        free(want);
    }
}

static void
test_random_sqr(void)
{
    for (unsigned i = 0; i < 1000; i++) {
        fp_t a, sq, aa;
        random_element(&a);

        fp_sqr(&sq, &a);
        fp_mul(&aa, &a, &a);
        assert(fp_equals(&sq, &aa) == UINT32_MAX);
    }
}

static void
test_decode_reduce(void)
{
    uint8_t one = 1;
    uint8_t zero[1] = { 0 };
    fp_t a, b;

    fp_decode_reduce(&a, &one, 1);
    fp_decode_reduce(&b, zero, 1);
    assert(fp_equals(&a, &b) == 0);

    fp_decode_reduce(&a, NULL, 0);
    assert(fp_equals(&a, &b) == UINT32_MAX);
}

int
main(void)
{
    test_roundtrip();
    test_equals();
    test_is_zero();
    test_decode_reduce();
    test_small_mul();
    test_double();
    test_half();
    test_n_sqr();
    test_sum_difference_of_products();
    test_batch_invert();
    test_mul_small();
    test_ct_helpers();
    test_random_add();
    test_random_sub();
    test_random_neg();
    test_random_mul();
    test_random_sqr();
    test_small_inv();
    test_random_inv();
    test_legendre();
    test_random_sqrt();

    puts("all tests passed");
    return 0;
}
