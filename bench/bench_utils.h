
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define BENCH_LOOPS 10000u
#define BENCH_EXPENSIVE_LOOPS 100u
#define BENCH_RUNS 20
#define BENCH_WARMUP 1000u

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
print_result(const char *name, uint64_t runs[BENCH_RUNS],
             unsigned loops, uint64_t ops)
{
    printf("  %-35s %8" PRIu64 " cycles\n",
           name, bench_value(runs, loops, ops));
}
