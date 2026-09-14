#include "fp2.h"
#include "bench_utils.h"

static void
make_inputs(fp2_t *a, fp2_t *b, fp2_t *c, fp2_t *d)
{
    fp2_set_small(a, 1);
    fp2_set_small(b, 2);
    fp2_set_small(c, 3);
    fp2_set_small(d, 4);
}

int
main(void)
{
    fp2_t a, b, c, d, r;
    uint8_t tmp[FP2_ENCODED_BYTES];
    uint64_t runs[BENCH_RUNS];

    make_inputs(&a, &b, &c, &d);

    /* Warm up code and instruction/data caches before measurements. */
    for (unsigned i = 0; i < BENCH_WARMUP; i++) {
        fp2_add(&r, &a, &b);
        fp2_mul(&a, &r, &b);
        fp2_sqr(&b, &a);
        fp2_half(&r, &a);
    }

    printf("\n--------------------------------------------------------------------------\n\n");
    printf("Benchmarking GF(p^2) field arithmetic (%u loops, %u runs)\n\n",
           BENCH_LOOPS, BENCH_RUNS);

    /* GF(p^2) addition */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_add(&a, &a, &b);
            fp2_add(&b, &b, &a);
            fp2_add(&a, &a, &b);
            fp2_add(&b, &b, &a);
            fp2_add(&a, &a, &b);
            fp2_add(&b, &b, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &b);
    print_result("GF(p^2) addition", runs, BENCH_LOOPS, 6);

    /* GF(p^2) subtraction */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_sub(&a, &a, &b);
            fp2_sub(&b, &b, &a);
            fp2_sub(&a, &a, &b);
            fp2_sub(&b, &b, &a);
            fp2_sub(&a, &a, &b);
            fp2_sub(&b, &b, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &b);
    print_result("GF(p^2) subtraction", runs, BENCH_LOOPS, 6);

    /* GF(p^2) negation */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_neg(&r, &a);
            fp2_neg(&a, &r);
            fp2_neg(&r, &a);
            fp2_neg(&a, &r);
            fp2_neg(&r, &a);
            fp2_neg(&a, &r);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &a);
    print_result("GF(p^2) negation", runs, BENCH_LOOPS, 6);

    /* GF(p^2) multiplication */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_mul(&a, &a, &b);
            fp2_mul(&b, &b, &a);
            fp2_mul(&a, &a, &b);
            fp2_mul(&b, &b, &a);
            fp2_mul(&a, &a, &b);
            fp2_mul(&b, &b, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &b);
    print_result("GF(p^2) multiplication", runs, BENCH_LOOPS, 6);

    /* GF(p^2) squaring */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_sqr(&a, &a);
            fp2_sqr(&a, &a);
            fp2_sqr(&a, &a);
            fp2_sqr(&a, &a);
            fp2_sqr(&a, &a);
            fp2_sqr(&a, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &a);
    print_result("GF(p^2) squaring", runs, BENCH_LOOPS, 6);

    /* GF(p^2) double */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_double(&a, &a);
            fp2_double(&a, &a);
            fp2_double(&a, &a);
            fp2_double(&a, &a);
            fp2_double(&a, &a);
            fp2_double(&a, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &a);
    print_result("GF(p^2) double", runs, BENCH_LOOPS, 6);

    /* GF(p^2) mul_small */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_mul_small(&a, &a, 123456789);
            fp2_mul_small(&a, &a, -123456789);
            fp2_mul_small(&a, &a, 123456789);
            fp2_mul_small(&a, &a, -123456789);
            fp2_mul_small(&a, &a, 123456789);
            fp2_mul_small(&a, &a, -123456789);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &a);
    print_result("GF(p^2) mul_small", runs, BENCH_LOOPS, 6);

    /* GF(p^2) half */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp2_half(&r, &a);
            fp2_half(&a, &b);
            fp2_half(&b, &c);
            fp2_half(&c, &d);
            fp2_half(&d, &r);
            fp2_half(&r, &a);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &r);
    print_result("GF(p^2) half", runs, BENCH_LOOPS, 6);

    /* GF(p^2) inversion. Additions keep the next input non-trivial. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            fp2_inv(&a, &a);
            fp2_add(&a, &a, &b);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &a);
    print_result("GF(p^2) inversion", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* GF(p^2) sqrt. Feed it a guaranteed square. */
    fp2_sqr(&r, &a);
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            fp2_sqrt(&a, &r);
        }
        runs[i] = cpucycles() - start;
    }
    fp2_encode(tmp, &a);
    print_result("GF(p^2) sqrt", runs, BENCH_EXPENSIVE_LOOPS, 1);

    printf("\nSanity byte: %u\n", tmp[0]);
    return 0;
}
