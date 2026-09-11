#include "fp_scott_bench.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* Simple cycle counter, as used by the original benchmark. */
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
make_inputs(scott_fp_t a, scott_fp_t b, scott_fp_t c, scott_fp_t d)
{
    scott_modint(1, a);
    scott_modint(2, b);
    scott_modint(3, c);
    scott_modint(4, d);
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
    scott_fp_t a, b, c, d, r, t;
    uint64_t runs[BENCH_RUNS];

    make_inputs(a, b, c, d);

    /* Warm up code and instruction/data caches before measurements. */
    for (unsigned i = 0; i < BENCH_WARMUP; i++) {
        scott_modadd(a, b, r);
        scott_modmul(a, r, a);
        scott_modsqr(a, b);
        scott_modmli(a, 2, r);
        scott_modmli(a, 123456789, r);
        scott_modmul(a, b, t);
        scott_modmul(c, d, r);
        scott_modadd(t, r, r);
        scott_modmul(a, b, t);
        scott_modmul(c, d, r);
        scott_modsub(t, r, r);
        scott_modhaf(a);
    }

    printf("\n--------------------------------------------------------------------------\n\n");
    printf("Benchmarking Mike Scott's modular arithmetic (%u loops, %u runs)\n\n",
           BENCH_LOOPS, BENCH_RUNS);

    /* GF(p) addition */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modadd(a, b, a);
            scott_modadd(b, a, b);
            scott_modadd(a, b, a);
            scott_modadd(b, a, b);
            scott_modadd(a, b, a);
            scott_modadd(b, a, b);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) addition", runs, BENCH_LOOPS, 6);

    /* GF(p) subtraction */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modsub(a, b, a);
            scott_modsub(b, a, b);
            scott_modsub(a, b, a);
            scott_modsub(b, a, b);
            scott_modsub(a, b, a);
            scott_modsub(b, a, b);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) subtraction", runs, BENCH_LOOPS, 6);

    /* GF(p) negation */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modneg(a, r);
            scott_modneg(r, a);
            scott_modneg(a, r);
            scott_modneg(r, a);
            scott_modneg(a, r);
            scott_modneg(r, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) negation", runs, BENCH_LOOPS, 6);

    /* GF(p) multiplication */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modmul(a, a, b);
            scott_modmul(b, b, a);
            scott_modmul(a, a, b);
            scott_modmul(b, b, a);
            scott_modmul(a, a, b);
            scott_modmul(b, b, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) multiplication", runs, BENCH_LOOPS, 6);

    /* GF(p) squaring */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modsqr(a, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) squaring", runs, BENCH_LOOPS, 1);

    /* GF(p) mul2: Scott's equivalent is modmli(a, 2, a). */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modmli(a, 2, a);
            scott_modmli(a, 2, a);
            scott_modmli(a, 2, a);
            scott_modmli(a, 2, a);
            scott_modmli(a, 2, a);
            scott_modmli(a, 2, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) mul2", runs, BENCH_LOOPS, 6);

    /* GF(p) mul_small: Scott's equivalent is modmli. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modmli(a, 123456789, a);
            scott_modmli(a, -123456789, a);
            scott_modmli(a, 123456789, a);
            scott_modmli(a, -123456789, a);
            scott_modmli(a, 123456789, a);
            scott_modmli(a, -123456789, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) mul_small", runs, BENCH_LOOPS, 6);

    /* GF(p) sum of products, there's no version of this for Scott, so this will be slow */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_sum_of_products(r, a, b, c, d);
            scott_sum_of_products(a, r, b, c, d);
            scott_sum_of_products(b, a, c, d, r);
            scott_sum_of_products(c, b, d, a, r);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) sum of products", runs, BENCH_LOOPS, 4);

    /* GF(p) difference of products, there's no version of this for Scott, so this will be slow */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_difference_of_products(r, a, b, c, d);
            scott_difference_of_products(a, r, b, c, d);
            scott_difference_of_products(b, a, c, d, r);
            scott_difference_of_products(c, b, d, a, r);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) difference of products", runs, BENCH_LOOPS, 4);

    /* GF(p) half */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            scott_modhaf(a);
            scott_modhaf(b);
            scott_modhaf(c);
            scott_modhaf(d);
            scott_modhaf(a);
            scott_modhaf(b);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) half", runs, BENCH_LOOPS, 6);

    /* GF(p) inversion. Additions keep the next input non-trivial. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            scott_modinv(a, r);
            scott_modadd(r, b, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) inversion", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* GF(p) sqrt. Feed it a guaranteed square. */
    scott_modsqr(a, r);
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            scott_modsqrt(r, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) sqrt", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* Legendre symbol. Scott's modqr returns a quadratic-residue boolean. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        volatile int32_t symbol = 0;
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            symbol ^= scott_modqr(a);
            symbol ^= scott_modqr(b);
        }
        runs[i] = cpucycles() - start;
        (void)symbol;
    }
    print_result("GF(p) Legendre", runs, BENCH_EXPENSIVE_LOOPS, 2);

    printf("\nNote: cycle counts are CPU/core dependent.\n");
    return 0;
}
