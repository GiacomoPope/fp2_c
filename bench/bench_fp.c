#include "fp.h"
#include "two_two_isogeny_chain.h"
#include "bench_utils.h"

static void
make_inputs(fp_t *a, fp_t *b, fp_t *c, fp_t *d)
{
    fp_set_small(a, 1);
    fp_set_small(b, 2);
    fp_set_small(c, 3);
    fp_set_small(d, 4);
}

static uint64_t rng_state = UINT64_C(0x123456789abcdef0);

static uint64_t
rng64(void)
{
    rng_state = rng_state * UINT64_C(6364136223846793005) +
                UINT64_C(1442695040888963407);
    return rng_state;
}

static void
fp_random_element(fp_t *x)
{
    uint8_t bytes[FP_ENCODED_BYTES + 32];
    for (size_t i = 0; i < sizeof(bytes); i += 8) {
        uint64_t w = rng64();
        for (size_t j = 0; j < 8 && i + j < sizeof(bytes); j++)
            bytes[i + j] = (uint8_t)(w >> (8 * j));
    }
    fp_decode_reduce(x, bytes, sizeof(bytes));
}

static void
make_random_theta(theta_t *t)
{
    fp_random_element(&t->x);
    fp_random_element(&t->y);
    fp_random_element(&t->z);
    fp_random_element(&t->t);
}

int
main(void)
{
    fp_t a, b, c, d, r;
    uint8_t tmp[FP_ENCODED_BYTES];
    uint64_t runs[BENCH_RUNS];

    make_inputs(&a, &b, &c, &d);

    /* Warm up code and instruction/data caches before measurements. */
    for (unsigned i = 0; i < BENCH_WARMUP; i++) {
        fp_add(&r, &a, &b);
        fp_mul(&a, &r, &b);
        fp_sqr(&b, &a);
        fp_sum_of_products(&r, &a, &b, &c, &d);
        fp_difference_of_products(&r, &a, &b, &c, &d);
        fp_half(&r, &a);
    }

    printf("\n--------------------------------------------------------------------------\n\n");
    printf("Benchmarking GF(p) field arithmetic (%u loops, %u runs)\n\n",
           BENCH_LOOPS, BENCH_RUNS);

    /* GF(p) addition */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_add(&a, &a, &b);
            fp_add(&b, &b, &a);
            fp_add(&a, &a, &b);
            fp_add(&b, &b, &a);
            fp_add(&a, &a, &b);
            fp_add(&b, &b, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &b);
    print_result("GF(p) addition", runs, BENCH_LOOPS, 6);

    /* GF(p) subtraction */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_sub(&a, &a, &b);
            fp_sub(&b, &b, &a);
            fp_sub(&a, &a, &b);
            fp_sub(&b, &b, &a);
            fp_sub(&a, &a, &b);
            fp_sub(&b, &b, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &b);
    print_result("GF(p) subtraction", runs, BENCH_LOOPS, 6);

    /* GF(p) negation */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_neg(&r, &a);
            fp_neg(&a, &r);
            fp_neg(&r, &a);
            fp_neg(&a, &r);
            fp_neg(&r, &a);
            fp_neg(&a, &r);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &a);
    print_result("GF(p) negation", runs, BENCH_LOOPS, 6);

    /* GF(p) multiplication */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_mul(&a, &a, &b);
            fp_mul(&b, &b, &a);
            fp_mul(&a, &a, &b);
            fp_mul(&b, &b, &a);
            fp_mul(&a, &a, &b);
            fp_mul(&b, &b, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &b);
    print_result("GF(p) multiplication", runs, BENCH_LOOPS, 6);

    /* GF(p) squaring */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_sqr(&a, &a);
            fp_sqr(&a, &a);
            fp_sqr(&a, &a);
            fp_sqr(&a, &a);
            fp_sqr(&a, &a);
            fp_sqr(&a, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &a);
    print_result("GF(p) squaring", runs, BENCH_LOOPS, 6);

    /* GF(p) double */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_double(&a, &a);
            fp_double(&a, &a);
            fp_double(&a, &a);
            fp_double(&a, &a);
            fp_double(&a, &a);
            fp_double(&a, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &a);
    print_result("GF(p) double", runs, BENCH_LOOPS, 6);

    /* GF(p) mul_small */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_mul_small(&a, &a, 123456789);
            fp_mul_small(&a, &a, -123456789);
            fp_mul_small(&a, &a, 123456789);
            fp_mul_small(&a, &a, -123456789);
            fp_mul_small(&a, &a, 123456789);
            fp_mul_small(&a, &a, -123456789);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &a);
    print_result("GF(p) mul_small", runs, BENCH_LOOPS, 6);

    /* GF(p) sum of products */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_sum_of_products(&r, &a, &b, &c, &d);
            fp_sum_of_products(&a, &r, &b, &c, &d);
            fp_sum_of_products(&b, &a, &c, &d, &r);
            fp_sum_of_products(&c, &b, &d, &a, &r);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &c);
    print_result("GF(p) sum of products", runs, BENCH_LOOPS, 4);

    /* GF(p) difference of products */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_difference_of_products(&r, &a, &b, &c, &d);
            fp_difference_of_products(&a, &r, &b, &c, &d);
            fp_difference_of_products(&b, &a, &c, &d, &r);
            fp_difference_of_products(&c, &b, &d, &a, &r);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &c);
    print_result("GF(p) difference of products", runs, BENCH_LOOPS, 4);

    /* GF(p) half */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_half(&r, &a);
            fp_half(&a, &b);
            fp_half(&b, &c);
            fp_half(&c, &d);
            fp_half(&d, &r);
            fp_half(&r, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &r);
    print_result("GF(p) half", runs, BENCH_LOOPS, 6);

    /* GF(p) inversion. Additions keep the next input non-trivial. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            fp_inv(&a, &a);
            fp_add(&a, &a, &b);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &a);
    print_result("GF(p) inversion", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* GF(p) sqrt. Feed it a guaranteed square. */
    fp_sqr(&r, &a);
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            fp_sqrt(&a, &r);
        }
        runs[i] = cpucycles() - start;
    }
    fp_encode(tmp, &a);
    print_result("GF(p) sqrt", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* Legendre symbol. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        volatile int32_t symbol = 0;
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            symbol ^= fp_legendre(&a);
            symbol ^= fp_legendre(&b);
        }
        runs[i] = cpucycles() - start;
        (void)symbol;
    }
    print_result("GF(p) Legendre", runs, BENCH_EXPENSIVE_LOOPS, 2);

    theta_t domain, k1, k2, theta_codomain;
    make_random_theta(&domain);
    make_random_theta(&k1);
    make_random_theta(&k2);

    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            two_two_isogeny_chain(&theta_codomain, &domain, &k1, &k2, 128);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) (2,2)-isogeny chain, e=128", runs, BENCH_EXPENSIVE_LOOPS, 1);

    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            two_two_isogeny_chain(&theta_codomain, &domain, &k1, &k2, 256);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) (2,2)-isogeny chain, e=256", runs, BENCH_EXPENSIVE_LOOPS, 1);

    printf("\nSanity byte: %u\n", tmp[0]);
    return 0;
}
