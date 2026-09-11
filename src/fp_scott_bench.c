/* 
 * Separate-translation-unit wrappers for the Scott benchmark.
 */
#include "fp_scott_bench.h"
#include "fp_scott.c"

void
scott_modint(int x, scott_fp_t a)
{
    modint(x, a);
}

void
scott_modadd(const scott_fp_t a, const scott_fp_t b, scott_fp_t r)
{
    modadd(a, b, r);
}

void
scott_modsub(const scott_fp_t a, const scott_fp_t b, scott_fp_t r)
{
    modsub(a, b, r);
}

void
scott_modneg(const scott_fp_t a, scott_fp_t r)
{
    modneg(a, r);
}

void
scott_modmul(const scott_fp_t a, const scott_fp_t b, scott_fp_t r)
{
    modmul(a, b, r);
}

void
scott_modsqr(const scott_fp_t a, scott_fp_t r)
{
    modsqr(a, r);
}

void
scott_modmli(const scott_fp_t a, int k, scott_fp_t r)
{
    modmli(a, k, r);
}

void
scott_modinv(const scott_fp_t a, scott_fp_t r)
{
    modinv(a, NULL, r);
}

void
scott_modsqrt(const scott_fp_t a, scott_fp_t r)
{
    modsqrt(a, NULL, r);
}

int
scott_modqr(const scott_fp_t a)
{
    return modqr(NULL, a);
}

void
scott_modcmv(int ctl, const scott_fp_t a, volatile scott_fp_t r)
{
    modcmv(ctl, a, r);
}

void
scott_modcsw(int ctl, volatile scott_fp_t a, volatile scott_fp_t b)
{
    modcsw(ctl, a, b);
}

void
scott_modhaf(scott_fp_t a)
{
    modhaf(a);
}

void
scott_sum_of_products(scott_fp_t r, const scott_fp_t a1, const scott_fp_t b1, const scott_fp_t a2, const scott_fp_t b2)
{
    scott_fp_t a, b;
    modmul(a1, b1, a);
    modmul(a2, b2, b);
    modadd(a, b, r);
}
void scott_difference_of_products(scott_fp_t r, const scott_fp_t a1, const scott_fp_t b1, const scott_fp_t a2, const scott_fp_t b2)
{
    scott_fp_t a, b;
    modmul(a1, b1, a);
    modmul(a2, b2, b);
    modsub(a, b, r);
}
