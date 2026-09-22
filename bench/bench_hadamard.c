#include "fp.h"
#include "theta_dim4.h"
#include "bench_utils.h"

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
random_theta_dim4(theta_dim4_t *t)
{
    for (int i = 0; i < 16; i++)
        fp_random_element(&t->coords[i]);
}

/* Same butterfly structure as theta_dim4_hadamard, but built from plain
   fp_add/fp_sub calls instead of fp_hadamard: the "naive" baseline. */
static void
theta_dim4_hadamard_naive(theta_dim4_t *out, const theta_dim4_t *in)
{
    /* Same in-place-after-stage-0 structure as theta_dim4_hadamard,
       kept identical here so the comparison isolates the fp_hadamard
       fusion itself rather than an unrelated copy-avoidance difference. */
    for (size_t i = 0; i < 8; i++) {
        fp_t sum, diff;
        fp_add(&sum, &in->coords[2 * i], &in->coords[2 * i + 1]);
        fp_sub(&diff, &in->coords[2 * i], &in->coords[2 * i + 1]);
        out->coords[2 * i] = sum;
        out->coords[2 * i + 1] = diff;
    }

    static const size_t strides[3] = { 2, 4, 8 };
    for (size_t s = 0; s < 3; s++) {
        size_t stride = strides[s];
        for (size_t base = 0; base < 16; base += stride * 2) {
            for (size_t k = 0; k < stride; k++) {
                size_t i = base + k;
                size_t j = i + stride;
                fp_t sum, diff;
                fp_add(&sum, &out->coords[i], &out->coords[j]);
                fp_sub(&diff, &out->coords[i], &out->coords[j]);
                out->coords[i] = sum;
                out->coords[j] = diff;
            }
        }
    }
}

int
main(void)
{
    fp_t a, b, r1, r2;
    theta_dim4_t coords;
    uint64_t runs[BENCH_RUNS];

    fp_random_element(&a);
    fp_random_element(&b);
    random_theta_dim4(&coords);

    for (unsigned i = 0; i < BENCH_WARMUP; i++) {
        fp_add(&r1, &a, &b);
        fp_sub(&r2, &a, &b);
        fp_hadamard(&r1, &r2, &a, &b);
    }

    printf("\n--------------------------------------------------------------------------\n\n");
    printf("Benchmarking GF(p) Hadamard (%u loops, %u runs)\n\n",
           BENCH_LOOPS, BENCH_RUNS);

    /* Single pair, naive: r1 = a+b, r2 = a-b via fp_add + fp_sub. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_add(&r1, &a, &b);
            fp_sub(&r2, &a, &b);
            fp_add(&a, &r1, &r2);
            fp_sub(&b, &r1, &r2);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("pair, naive add+sub", runs, BENCH_LOOPS, 4);

    /* Single pair, fused fp_hadamard. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_LOOPS; n++) {
            fp_hadamard(&r1, &r2, &a, &b);
            fp_hadamard(&a, &b, &r1, &r2);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("pair, fused fp_hadamard", runs, BENCH_LOOPS, 2);

    /* dim-4 (16-coordinate) transform, naive add+sub. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            theta_dim4_hadamard_naive(&coords, &coords);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("dim-4, naive add+sub", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* dim-4 (16-coordinate) transform, fused fp_hadamard. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            theta_dim4_hadamard(&coords, &coords);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("dim-4, fused fp_hadamard", runs, BENCH_EXPENSIVE_LOOPS, 1);

    /* dim-4 (16-coordinate) squaring: 16 independent fp_sqr calls. */
    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            theta_dim4_square(&coords, &coords);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("dim-4, square (16x fp_sqr)", runs, BENCH_EXPENSIVE_LOOPS, 1);

    uint8_t tmp[FP_ENCODED_BYTES];
    fp_encode(tmp, &coords.coords[0]);
    printf("\nSanity byte: %u\n", tmp[0]);
    return 0;
}
