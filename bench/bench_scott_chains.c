/*
 * Benchmarks the same isogeny chain code as bench_fp.c/bench_fp2.c, but
 * compiled against Mike Scott's field arithmetic instead of our own. This
 * works unmodified because fp2.c, two_two_isogeny_chain.c and
 * four_isogeny_chain.c only ever talk to the fp_t API declared in fp.h;
 * -I ordering swaps that header for include/scott/fp.h (see that file),
 * which backs fp_t with Scott's spint/modXXX primitives instead of our
 * generated fp.c.
 */
#include "fp2.h"
#include "two_two_isogeny_chain.h"
#include "four_isogeny_chain.h"
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
random_bytes(uint8_t *out, size_t len)
{
    for (size_t i = 0; i < len; i += 8) {
        uint64_t w = rng64();
        for (size_t j = 0; j < 8 && i + j < len; j++)
            out[i + j] = (uint8_t)(w >> (8 * j));
    }
}

static void
fp_random_element(fp_t *x)
{
    uint8_t bytes[FP_ENCODED_BYTES + 32];
    random_bytes(bytes, sizeof(bytes));
    fp_decode_reduce(x, bytes, sizeof(bytes));
}

static void
fp2_random_element(fp2_t *x)
{
    uint8_t bytes[FP2_ENCODED_BYTES + 32];
    random_bytes(bytes, sizeof(bytes));
    fp2_decode_reduce(x, bytes, sizeof(bytes));
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
    theta_t domain, k1, k2, theta_codomain;
    fp2_t A, kernel_X, kernel_Z, out_X, out_Z;
    uint64_t runs[BENCH_RUNS];

    make_random_theta(&domain);
    make_random_theta(&k1);
    make_random_theta(&k2);

    fp2_random_element(&A);
    fp2_random_element(&kernel_X);
    fp2_random_element(&kernel_Z);

    printf("\n--------------------------------------------------------------------------\n\n");
    printf("Benchmarking isogeny chains over Mike Scott's modular arithmetic\n\n");

    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            two_two_isogeny_chain(&theta_codomain, &domain, &k1, &k2, 128);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("(2,2)-isogeny chain over Fp, e=128", runs, BENCH_EXPENSIVE_LOOPS, 1);

    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            two_two_isogeny_chain(&theta_codomain, &domain, &k1, &k2, 256);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("(2,2)-isogeny chain over Fp, e=256", runs, BENCH_EXPENSIVE_LOOPS, 1);

    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            four_isogeny_chain(&out_X, &out_Z, &A, &kernel_X, &kernel_Z, 128);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("4-isogeny chain over Fp2, e=128", runs, BENCH_EXPENSIVE_LOOPS, 1);

    for (int i = 0; i < BENCH_RUNS; i++) {
        uint64_t start = cpucycles();
        for (unsigned n = 0; n < BENCH_EXPENSIVE_LOOPS; n++) {
            four_isogeny_chain(&out_X, &out_Z, &A, &kernel_X, &kernel_Z, 256);
        }
        runs[i] = cpucycles() - start;
    }
    print_result("4-isogeny chain over Fp2, e=256", runs, BENCH_EXPENSIVE_LOOPS, 1);

    printf("\nNote: cycle counts are CPU/core dependent.\n");
    return 0;
}
