#include "fp.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* Simple cycle counter, as used by the sqisign benchmark. */
#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>

static inline uint64_t
cpucycles(void)
{
    unsigned int aux;
    _mm_lfence();
    uint64_t r = __rdtscp(&aux);
    _mm_lfence();
    return r;
}
#elif defined(__aarch64__)
static inline uint64_t
cpucycles(void)
{
    uint64_t x;
    __asm__ __volatile__("isb\n\t" "mrs %0, cntvct_el0" : "=r"(x));
    return x;
}
#elif defined(__riscv) && (__riscv_xlen == 64)
static inline uint64_t
cpucycles(void)
{
    uint64_t x;
    __asm__ __volatile__("rdcycle %0" : "=r"(x));
    return x;
}
#else
#error "Unsupported architecture for cycle counting."
#endif

#define BENCH_LOOPS 100000u
#define BENCH_EXPENSIVE_LOOPS 1000u
#define BENCH_RUNS 20
#define BENCH_WARMUP 1000u

static int
cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* Return the median-ish value after discarding the first 10 runs. */
static uint64_t
bench_value(uint64_t runs[BENCH_RUNS], unsigned loops,
            uint64_t operations_per_loop)
{
    qsort(runs + BENCH_RUNS / 2, BENCH_RUNS / 2,
          sizeof runs[0], cmp_u64);
    return runs[BENCH_RUNS / 2 + 2] /
           ((uint64_t)loops * operations_per_loop);
}

static void
make_inputs(fp_t *a, fp_t *b, fp_t *c, fp_t *d)
{
    uint8_t one[FP_ENCODED_LENGTH] = { 0 };
    uint8_t two[FP_ENCODED_LENGTH] = { 0 };
    uint8_t three[FP_ENCODED_LENGTH] = { 0 };
    uint8_t four[FP_ENCODED_LENGTH] = { 0 };
    one[0] = 1;
    two[0] = 2;
    three[0] = 3;
    four[0] = 4;
    if (fp_decode(a, one) != UINT32_MAX ||
        fp_decode(b, two) != UINT32_MAX ||
        fp_decode(c, three) != UINT32_MAX ||
        fp_decode(d, four) != UINT32_MAX) {
        fprintf(stderr, "failed to decode benchmark inputs\n");
        exit(EXIT_FAILURE);
    }
}

static void
print_result(const char *name, uint64_t runs[BENCH_RUNS],
             unsigned loops, uint64_t ops)
{
    printf("  %-32s %8" PRIu64 " cycles\n",
           name, bench_value(runs, loops, ops));
}

int
main(void)
{
    fp_t a, b, c, d, r;
    uint8_t tmp[FP_ENCODED_LENGTH];
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

    printf("\nSanity byte: %u\n", tmp[0]);
    printf("\nNote: cycle counts are CPU/core dependent.\n");
    return 0;
}
