#include "fp_scott.c"

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
make_inputs(spint *a, spint *b, spint *c, spint *d)
{
    modint(1, a);
    modint(2, b);
    modint(3, c);
    modint(4, d);
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
    spint a[Nlimbs], b[Nlimbs], c[Nlimbs], d[Nlimbs], r[Nlimbs], t[Nlimbs];
    uint64_t runs[BENCH_RUNS];

    make_inputs(a, b, c, d);

    /* Warm up code and instruction/data caches before measurements. */
    for (unsigned i = 0; i < BENCH_WARMUP; i++) {
        modadd(a, b, r);
        modmul(a, r, a);
        modsqr(a, b);
        modmli(a, 2, r);
        modmli(a, 123456789, r);
        modmul(a, b, t);
        modmul(c, d, r);
        modadd(t, r, r);
        modmul(a, b, t);
        modmul(c, d, r);
        modsub(t, r, r);
        modhaf(a);
    }

    printf("\n--------------------------------------------------------------------------\n\n");
    printf("Benchmarking Mike Scott's modular arithmetic (%u loops, %u runs)\n\n",
           BENCH_LOOPS, BENCH_RUNS);

    /* GF(p) addition */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modadd(a, b, a);
            modadd(b, a, b);
            modadd(a, b, a);
            modadd(b, a, b);
            modadd(a, b, a);
            modadd(b, a, b);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) addition", runs, BENCH_LOOPS, 6);

    /* GF(p) subtraction */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modsub(a, b, a);
            modsub(b, a, b);
            modsub(a, b, a);
            modsub(b, a, b);
            modsub(a, b, a);
            modsub(b, a, b);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) subtraction", runs, BENCH_LOOPS, 6);

    /* GF(p) negation */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modneg(a, r);
            modneg(r, a);
            modneg(a, r);
            modneg(r, a);
            modneg(a, r);
            modneg(r, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) negation", runs, BENCH_LOOPS, 6);

    /* GF(p) multiplication, serial. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modmul(a, a, b);
            modmul(b, b, a);
            modmul(a, a, b);
            modmul(b, b, a);
            modmul(a, a, b);
            modmul(b, b, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) multiplication, serial", runs, BENCH_LOOPS, 6);

    /* GF(p) multiplication, parallel. */
    spint mx[4][Nlimbs], my[4][Nlimbs];
    for (int l = 0; l < 4; l++) {
        modint(l + 1, mx[l]);
        modint(l + 5, my[l]);
    }
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            for (int l = 0; l < 4; l++)
                modmul(mx[l], my[l], mx[l]);
            for (int l = 0; l < 4; l++)
                modmul(my[l], mx[l], my[l]);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) multiplication, parallel", runs, BENCH_LOOPS, 8);

    /* GF(p) squaring, serial. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modsqr(a, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) squaring, serial", runs, BENCH_LOOPS, 1);

    /* GF(p) squaring, parallel. */
    spint sx[4][Nlimbs];
    for (int l = 0; l < 4; l++)
        modint(l + 1, sx[l]);
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            for (int l = 0; l < 4; l++)
                modsqr(sx[l], sx[l]);
            for (int l = 0; l < 4; l++)
                modsqr(sx[l], sx[l]);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) squaring, parallel", runs, BENCH_LOOPS, 8);

    /* GF(p) mul2: Scott's equivalent is modmli(a, 2, a). */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modmli(a, 2, a);
            modmli(a, 2, a);
            modmli(a, 2, a);
            modmli(a, 2, a);
            modmli(a, 2, a);
            modmli(a, 2, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) mul2", runs, BENCH_LOOPS, 6);

    /* GF(p) mul_small: Scott's equivalent is modmli. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modmli(a, 123456789, a);
            modmli(a, -123456789, a);
            modmli(a, 123456789, a);
            modmli(a, -123456789, a);
            modmli(a, 123456789, a);
            modmli(a, -123456789, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) mul_small", runs, BENCH_LOOPS, 6);

    /* GF(p) half */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            modhaf(a);
            modhaf(b);
            modhaf(c);
            modhaf(d);
            modhaf(a);
            modhaf(b);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) half", runs, BENCH_LOOPS, 6);

    /* GF(p) inversion. Additions keep the next input non-trivial. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            modinv(a, NULL, r);
            modadd(r, b, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) inversion", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* GF(p) sqrt. Feed it a guaranteed square. */
    modsqr(a, r);
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            modsqrt(r, NULL, a);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("GF(p) sqrt", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* Legendre symbol. Scott's modqr returns a quadratic-residue boolean. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        volatile int32_t symbol = 0;
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            symbol ^= modqr(NULL, a);
            symbol ^= modqr(NULL, b);
        }
        runs[i] = cpucycles() - start;
        (void)symbol;
    }
    print_result("GF(p) Legendre", runs, BENCH_EXPENSIVE_LOOPS, 2);

    printf("\nNote: cycle counts are CPU/core dependent.\n");
    return 0;
}
