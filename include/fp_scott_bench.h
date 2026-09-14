#ifndef FP_SCOTT_BENCH_H
#define FP_SCOTT_BENCH_H

#include <stdint.h>

typedef uint64_t scott_fp_t[5];

void scott_modint(int x, scott_fp_t a);
void scott_modadd(const scott_fp_t a, const scott_fp_t b, scott_fp_t r);
void scott_modsub(const scott_fp_t a, const scott_fp_t b, scott_fp_t r);
void scott_modneg(const scott_fp_t a, scott_fp_t r);
void scott_modmul(const scott_fp_t a, const scott_fp_t b, scott_fp_t r);
void scott_modsqr(const scott_fp_t a, scott_fp_t r);
void scott_modmli(const scott_fp_t a, int k, scott_fp_t r);
void scott_modinv(const scott_fp_t a, scott_fp_t r);
void scott_modsqrt(const scott_fp_t a, scott_fp_t r);
int  scott_modqr(const scott_fp_t a);
void scott_modcmv(int ctl, const scott_fp_t a, volatile scott_fp_t r);
void scott_modcsw(int ctl, volatile scott_fp_t a, volatile scott_fp_t b);
void scott_modhaf(scott_fp_t a);

#endif
