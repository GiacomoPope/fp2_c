#include <stdio.h>
#include <string.h>

#include "fp.h"
#include "fp2.h"

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
fp2_random_element(fp2_t *x)
{
    uint8_t bytes[FP2_ENCODED_BYTES + 32];
    random_bytes(bytes, sizeof(bytes));
    fp2_decode_reduce(x, bytes, sizeof(bytes));
}

static uint32_t
u32_random_test()
{
    return (uint32_t)rng64();
}

int
fp2_test(int iterations)
{
    int n, passed;
    fp2_t a, b, c, d, e, f;

    printf("\n-------------------------------------------------------------------------------------"
           "-------------------\n\n");
    printf("Testing arithmetic over GF(p^2): \n\n");

    // Addition in GF(p^2)
    passed = 1;
    for (n = 0; n < iterations; n++) {
        fp2_random_element(&a);
        fp2_random_element(&b);
        fp2_random_element(&c);
        fp2_random_element(&d);

        fp2_add(&d, &a, &b);
        fp2_add(&e, &d, &c); // e = (a+b)+c
        fp2_add(&d, &b, &c);
        fp2_add(&f, &d, &a); // f = a+(b+c)
        if (fp2_equals(&e, &f) == 0) {
            passed = 0;
            break;
        }

        fp2_add(&d, &a, &b); // d = a+b
        fp2_add(&e, &b, &a); // e = b+a
        if (fp2_equals(&d, &e) == 0) {
            passed = 0;
            break;
        }

        fp2_set_zero(&b);
        fp2_add(&d, &a, &b); // d = a+0
        if (fp2_equals(&d, &a) == 0) {
            passed = 0;
            break;
        }

        fp2_neg(&d, &a);
        fp2_add(&e, &a, &d); // e = a+(-a)
        if (fp2_is_zero(&e) == 0) {
            passed = 0;
            break;
        }
    }
    if (passed == 1)
        printf("  GF(p^2) addition tests ............................................ PASSED");
    else {
        printf("  GF(p^2) addition tests... FAILED");
        printf("\n");
        return -1;
    }
    printf("\n");

    // Subtraction in GF(p^2)
    passed = 1;
    for (n = 0; n < iterations; n++) {
        fp2_random_element(&a);
        fp2_random_element(&b);
        fp2_random_element(&c);
        fp2_random_element(&d);

        fp2_sub(&d, &a, &b);
        fp2_sub(&e, &d, &c); // e = (a-b)-c
        fp2_add(&d, &b, &c);
        fp2_sub(&f, &a, &d); // f = a-(b+c)
        if (fp2_equals(&e, &f) == 0) {
            passed = 0;
            break;
        }

        fp2_sub(&d, &a, &b); // d = a-b
        fp2_sub(&e, &b, &a);
        fp2_neg(&e, &e); // e = -(b-a)
        if (fp2_equals(&d, &e) == 0) {
            passed = 0;
            break;
        }

        fp2_set_zero(&b);
        fp2_sub(&d, &a, &b); // d = a-0
        if (fp2_equals(&d, &a) == 0) {
            passed = 0;
            break;
        }

        fp2_sub(&e, &a, &a); // e = a+(-a)
        if (fp2_is_zero(&e) == 0) {
            passed = 0;
            break;
        }
    }
    if (passed == 1)
        printf("  GF(p^2) subtraction tests ......................................... PASSED");
    else {
        printf("  GF(p^2) subtraction tests... FAILED");
        printf("\n");
        return -1;
    }
    printf("\n");

    // Multiplication in GF(p^2)
    passed = 1;
    for (n = 0; n < iterations; n++) {
        fp2_random_element(&a);
        fp2_random_element(&b);
        fp2_random_element(&c);

        fp2_mul(&d, &a, &b);
        fp2_mul(&e, &d, &c); // e = (a*b)*c
        fp2_mul(&d, &b, &c);
        fp2_mul(&f, &d, &a); // f = a*(b*c)
        if (fp2_equals(&e, &f) == 0) {
            passed = 0;
            break;
        }

        fp2_add(&d, &b, &c);
        fp2_mul(&e, &a, &d); // e = a*(b+c)
        fp2_mul(&d, &a, &b);
        fp2_mul(&f, &a, &c);
        fp2_add(&f, &d, &f); // f = a*b+a*c
        if (fp2_equals(&e, &f) == 0) {
            passed = 0;
            break;
        }

        fp2_mul(&d, &a, &b); // d = a*b
        fp2_mul(&e, &b, &a); // e = b*a
        if (fp2_equals(&d, &e) == 0) {
            passed = 0;
            break;
        }

        fp2_set_one(&b);
        fp2_mul(&d, &a, &b); // d = a*1
        if (fp2_equals(&a, &d) == 0) {
            passed = 0;
            break;
        }

        fp2_set_zero(&b);
        fp2_mul(&d, &a, &b); // d = a*0
        if (fp2_is_zero(&d) == 0) {
            passed = 0;
            break;
        }
    }
    if (passed == 1)
        printf("  GF(p^2) multiplication tests ...................................... PASSED");
    else {
        printf("  GF(p^2) multiplication tests... FAILED");
        printf("\n");
        return -1;
    }
    printf("\n");

    // Test mul small
    passed = 1;
    for (n = 0; n < iterations; n++) {
        fp2_random_element(&a);
        uint32_t val = u32_random_test();

        // Multiply by a small value
        fp2_mul_small(&b, &a, val);

        // Convert and multiply as a fp val
        fp2_set_small(&c, val);
        fp2_mul(&d, &a, &c);

        // Values should be the same
        if (fp2_equals(&b, &d) == 0) {
            passed = 0;
            break;
        }
    }
    if (passed == 1)
        printf("  GF(p^2) mul small test ............................................ PASSED");
    else {
        printf("  GF(p^2) mul small tests... FAILED");
        printf("\n");
        return -1;
    }
    printf("\n");

    // Squaring in GF(p^2)
    passed = 1;
    for (n = 0; n < iterations; n++) {
        fp2_random_element(&a);
        fp2_sqr(&b, &a);     // b = a^2
        fp2_mul(&c, &a, &a); // c = a*a
        if (fp2_equals(&b, &c) == 0) {
            passed = 0;
            break;
        }

        fp2_set_zero(&a);
        fp2_sqr(&d, &a); // d = 0^2
        if (fp2_is_zero(&d) == 0) {
            passed = 0;
            break;
        }
    }
    if (passed == 1)
        printf("  GF(p^2) squaring tests............................................. PASSED");
    else {
        printf("  GF(p^2) squaring tests... FAILED");
        printf("\n");
        return -1;
    }
    printf("\n");

    // Inversion in GF(p^2)
    passed = 1;
    for (n = 0; n < iterations; n++) {
        fp2_random_element(&a);

        fp2_set_one(&d);
        fp2_copy(&b, &a);
        fp2_inv(&a, &a);
        fp2_mul(&c, &a, &b); // c = a*a^-1
        if (fp2_equals(&c, &d) == 0) {
            passed = 0;
            break;
        }

        fp2_set_zero(&a);
        fp2_set_zero(&d);
        fp2_inv(&a, &a); // c = 0^-1
        if (fp2_equals(&a, &d) == 0) {
            passed = 0;
            break;
        }
    }
    if (passed == 1)
        printf("  GF(p^2) inversion tests............................................ PASSED");
    else {
        printf("  GF(p^2) inversion tests... FAILED");
        printf("\n");
        return -1;
    }
    printf("\n");

    // Square root and square detection in GF(p^2)
    passed = 1;
    for (n = 0; n < iterations; n++) {
        fp2_random_element(&a);

        fp2_sqr(&c, &a); // c = a^2
        if (fp2_is_square(&c) == 0) {
            passed = 0;
            break;
        }

        fp2_sqrt(&c, &c); // c = a = sqrt(c)
        fp2_neg(&d, &c);
        if ((fp2_equals(&a, &c) == 0) && (fp2_equals(&a, &d) == 0)) {
            passed = 0;
            break;
        }
    }
    if (passed == 1)
        printf("  Square root, square tests.......................................... PASSED");
    else {
        printf("  Square root, square tests... FAILED");
        printf("\n");
        return -1;
    }
    printf("\n");

    return 0;
}

int
main(void)
{
    if (!fp2_test(1000)) {
        puts("all tests passed");
    }
    return 0;
}
