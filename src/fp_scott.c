// Automatically generated modular arithmetic C code
// Command line : python monty.py 64
// 0x4ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff Python
// Script by Mike Scott (Technology Innovation Institute, UAE, 2025)

#include <stdint.h>
#include <stdio.h>

#define sspint int64_t
#define spint uint64_t
#define dpint __uint128_t
#define sdpint __int128_t
#define Wordlength 64
#define Nlimbs 5
#define Radix 51
#define Nbits 251
#define Nbytes 32

#define MONTGOMERY
static spint inline prop(spint *n) {
  int i;
  spint mask = ((spint)1 << 51u) - (spint)1;
  sspint carry = (sspint)n[0];
  carry >>= 51u;
  n[0] &= mask;
  for (i = 1; i < 4; i++) {
    carry += (sspint)n[i];
    n[i] = (spint)carry & mask;
    carry >>= 51u;
  }
  n[4] += (spint)carry;
  return -((n[4] >> 1) >> 62u);
}
static spint inline flatten(spint *n) {
  spint carry = prop(n);
  n[0] -= (spint)1u & carry;
  n[4] += ((spint)0x500000000000u) & carry;
  (void)prop(n);
  return (carry & 1);
}
static spint inline modfsb(spint *n) {
  n[0] += (spint)1u;
  n[4] -= (spint)0x500000000000u;
  return flatten(n);
}
static void inline modadd(const spint *a, const spint *b, spint *n) {
  spint carry;
  n[0] = a[0] + b[0]; n[1] = a[1] + b[1]; n[2] = a[2] + b[2];
  n[3] = a[3] + b[3]; n[4] = a[4] + b[4];
  n[0] += (spint)2u; n[4] -= (spint)0xa00000000000u;
  carry = prop(n);
  n[0] -= (spint)2u & carry;
  n[4] += ((spint)0xa00000000000u) & carry;
  (void)prop(n);
}
static void inline modsub(const spint *a, const spint *b, spint *n) {
  spint carry;
  n[0] = a[0] - b[0]; n[1] = a[1] - b[1]; n[2] = a[2] - b[2];
  n[3] = a[3] - b[3]; n[4] = a[4] - b[4];
  carry = prop(n);
  n[0] -= (spint)2u & carry;
  n[4] += ((spint)0xa00000000000u) & carry;
  (void)prop(n);
}
static void inline modneg(const spint *b, spint *n) {
  spint carry;
  n[0] = (spint)0 - b[0]; n[1] = (spint)0 - b[1]; n[2] = (spint)0 - b[2];
  n[3] = (spint)0 - b[3]; n[4] = (spint)0 - b[4];
  carry = prop(n);
  n[0] -= (spint)2u & carry;
  n[4] += ((spint)0xa00000000000u) & carry;
  (void)prop(n);
}
static void inline modmul(const spint *a, const spint *b, spint *c) {
  dpint t = 0;
  spint p4 = 0x500000000000u;
  spint q = ((spint)1 << 51u);
  spint mask = (spint)(q - (spint)1);
  t += (dpint)a[0] * b[0]; spint v0 = ((spint)t & mask); t >>= 51;
  t += (dpint)a[0] * b[1]; t += (dpint)a[1] * b[0]; spint v1 = ((spint)t & mask); t >>= 51;
  t += (dpint)a[0] * b[2]; t += (dpint)a[1] * b[1]; t += (dpint)a[2] * b[0]; spint v2 = ((spint)t & mask); t >>= 51;
  t += (dpint)a[0] * b[3]; t += (dpint)a[1] * b[2]; t += (dpint)a[2] * b[1]; t += (dpint)a[3] * b[0]; spint v3 = ((spint)t & mask); t >>= 51;
  t += (dpint)a[0] * b[4]; t += (dpint)a[1] * b[3]; t += (dpint)a[2] * b[2]; t += (dpint)a[3] * b[1]; t += (dpint)a[4] * b[0];
  t += (dpint)v0 * p4; spint v4 = ((spint)t & mask); t >>= 51;
  t += (dpint)a[1] * b[4]; t += (dpint)a[2] * b[3]; t += (dpint)a[3] * b[2]; t += (dpint)a[4] * b[1];
  t += (dpint)v1 * p4; c[0] = ((spint)t & mask); t >>= 51;
  t += (dpint)a[2] * b[4]; t += (dpint)a[3] * b[3]; t += (dpint)a[4] * b[2]; t += (dpint)v2 * p4;
  c[1] = ((spint)t & mask); t >>= 51;
  t += (dpint)a[3] * b[4]; t += (dpint)a[4] * b[3]; t += (dpint)v3 * p4;
  c[2] = ((spint)t & mask); t >>= 51;
  t += (dpint)a[4] * b[4]; t += (dpint)v4 * p4;
  c[3] = ((spint)t & mask); t >>= 51; c[4] = (spint)t;
}
static void inline modsqr(const spint *a, spint *c) {
  dpint tot; dpint t = 0; spint p4 = 0x500000000000u;
  spint q = ((spint)1 << 51u); spint mask = (spint)(q - (spint)1);
  tot = (dpint)a[0] * a[0]; t = tot; spint v0 = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[0] * a[1]; tot *= 2; t += tot; spint v1 = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[0] * a[2]; tot *= 2; tot += (dpint)a[1] * a[1]; t += tot; spint v2 = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[0] * a[3]; tot += (dpint)a[1] * a[2]; tot *= 2; t += tot; spint v3 = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[0] * a[4]; tot += (dpint)a[1] * a[3]; tot *= 2; tot += (dpint)a[2] * a[2]; t += tot;
  t += (dpint)v0 * p4; spint v4 = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[1] * a[4]; tot += (dpint)a[2] * a[3]; tot *= 2; t += tot; t += (dpint)v1 * p4;
  c[0] = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[2] * a[4]; tot *= 2; tot += (dpint)a[3] * a[3]; t += tot; t += (dpint)v2 * p4;
  c[1] = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[3] * a[4]; tot *= 2; t += tot; t += (dpint)v3 * p4;
  c[2] = ((spint)t & mask); t >>= 51;
  tot = (dpint)a[4] * a[4]; t += tot; t += (dpint)v4 * p4;
  c[3] = ((spint)t & mask); t >>= 51; c[4] = (spint)t;
}
static void inline modcpy(const spint *a, spint *c) { for (int i = 0; i < 5; i++) c[i] = a[i]; }
static void modnsqr(spint *a, int n) { for (int i = 0; i < n; i++) modsqr(a, a); }
static void modpro(const spint *w, spint *z) {
  spint x[5], t0[5], t1[5], t2[5], t3[5], t4[5]; modcpy(w, x);
  modsqr(x, z); modmul(x, z, t0); modsqr(t0, z); modmul(x, z, z); modsqr(z, t1); modsqr(t1, t3); modsqr(t3, t2);
  modcpy(t2, t4); modnsqr(t4, 3); modmul(t2, t4, t2); modcpy(t2, t4); modnsqr(t4, 6); modmul(t2, t4, t2);
  modcpy(t2, t4); modnsqr(t4, 2); modmul(t3, t4, t3); modnsqr(t3, 13); modmul(t2, t3, t2); modcpy(t2, t3);
  modnsqr(t3, 27); modmul(t2, t3, t2); modmul(z, t2, z); modcpy(z, t2); modnsqr(t2, 4); modmul(t1, t2, t1);
  modmul(t0, t1, t0); modmul(t1, t0, t1); modmul(t0, t1, t0); modmul(t1, t0, t2); modmul(t0, t2, t0); modmul(t1, t0, t1);
  modnsqr(t1, 63); modmul(t0, t1, t1); modnsqr(t1, 64); modmul(t0, t1, t0); modnsqr(t0, 57); modmul(z, t0, z);
}
static void modinv(const spint *x, const spint *h, spint *z) {
  spint s[5], t[5]; if (h == NULL) modpro(x, t); else modcpy(h, t); modcpy(x, s); modnsqr(t, 2); modmul(s, t, z);
}
static void nres(const spint *m, spint *n) {
  const spint c[5] = {0x4cccccccccf5cu, 0x1999999999999u, 0x3333333333333u, 0x6666666666666u, 0xcccccccccccu};
  modmul(m, c, n);
}
static void redc(const spint *n, spint *m) { spint c[5] = {1,0,0,0,0}; modmul(n, c, m); (void)modfsb(m); }
static int modis1(const spint *a) {
  spint c[5], c0, d = 0; redc(a, c); for (int i = 1; i < 5; i++) d |= c[i]; c0 = c[0];
  return ((spint)1 & ((d - (spint)1) >> 51u) & (((c0 ^ (spint)1) - (spint)1) >> 51u));
}
static int modis0(const spint *a) { spint c[5], d = 0; redc(a, c); for (int i = 0; i < 5; i++) d |= c[i]; return ((spint)1 & ((d - (spint)1) >> 51u)); }
static void modzer(spint *a) { for (int i = 0; i < 5; i++) a[i] = 0; }
static void modone(spint *a) { a[0] = 1; for (int i = 1; i < 5; i++) a[i] = 0; nres(a, a); }
static void modint(int x, spint *a) { a[0] = (spint)x; for (int i = 1; i < 5; i++) a[i] = 0; nres(a, a); }
static void inline modmli(const spint *a, int b, spint *c) {
  spint p4 = 0x500000000000u, mask = ((spint)1 << 51u) - 1, q, h, r = 0xccccccccccccc; dpint t = 0;
  t += (dpint)a[0] * (dpint)b; c[0] = (spint)t & mask; t >>= 51;
  t += (dpint)a[1] * (dpint)b; c[1] = (spint)t & mask; t >>= 51;
  t += (dpint)a[2] * (dpint)b; c[2] = (spint)t & mask; t >>= 51;
  t += (dpint)a[3] * (dpint)b; c[3] = (spint)t & mask; t >>= 51;
  t += (dpint)a[4] * (dpint)b; c[4] = (spint)t;
  h = (spint)(t >> 34u); q = (spint)(((dpint)h * (dpint)r) >> 64u); c[0] += q; c[4] -= q * p4;
}
static int modqr(const spint *h, const spint *x) { spint r[5]; if (h == NULL) { modpro(x, r); modsqr(r, r); } else modsqr(h, r); modmul(r, x, r); return modis1(r) | modis0(x); }
static void __attribute__((noinline)) modcmv(int b, const spint *g, volatile spint *f) {
  spint c0, c1, s, t, w, aux; static spint R = 0; R += 0x3cc3c33c5aa5a55au; w = R; c0 = (~b) & (w + 1); c1 = b + w;
  for (int i = 0; i < 5; i++) { s = g[i]; t = f[i]; f[i] = aux = c0 * t + c1 * s; f[i] = aux - w * (t + s); }
}
static void __attribute__((noinline)) modcsw(int b, volatile spint *g, volatile spint *f) {
  spint c0, c1, s, t, w, v, aux; static spint R = 0; R += 0x3cc3c33c5aa5a55au; w = R; c0 = (~b) & (w + 1); c1 = b + w;
  for (int i = 0; i < 5; i++) { s = g[i]; t = f[i]; v = w * (t + s); f[i] = aux = c0 * t + c1 * s; f[i] = aux - v; g[i] = aux = c0 * s + c1 * t; g[i] = aux - v; }
}
static void modsqrt(const spint *x, const spint *h, spint *r) { spint s[5], y[5]; if (h == NULL) modpro(x, y); else modcpy(h, y); modmul(y, x, s); modcpy(s, r); }
static void modshl(unsigned int n, spint *a) {
  a[4] = (a[4] << n) + (a[3] >> (51u - n));
  for (int i = 3; i > 0; i--) a[i] = ((a[i] << n) & 0x7ffffffffffffu) + (a[i - 1] >> (51u - n));
  a[0] = (a[0] << n) & 0x7ffffffffffffu;
}
static int modshr(unsigned int n, spint *a) {
  int r = a[0] & (((spint)1 << n) - 1); for (int i = 0; i < 4; i++) a[i] = (a[i] >> n) + ((a[i + 1] << (51u - n)) & 0x7ffffffffffffu); a[4] >>= n; return r;
}
static void modhaf(spint *n) {
  int lsb; spint t[5]; (void)prop(n); modcpy(n, t); lsb = modshr(1, t); n[0] -= 1; n[4] += 0x500000000000u; (void)prop(n); modshr(1, n); modcmv(1 - lsb, t, n);
}
static void mod2r(unsigned int r, spint *a) { unsigned int n = r / 51u, m = r % 51u; modzer(a); if (r >= 32 * 8) return; a[n] = ((spint)1 << m); nres(a, a); }
static void modexp(const spint *a, char *b) { spint c[5]; redc(a, c); for (int i = 31; i >= 0; i--) { b[i] = c[0] & 0xff; (void)modshr(8, c); } }
static int modimp(const char *b, spint *a) { int res; for (int i = 0; i < 5; i++) a[i] = 0; for (int i = 0; i < 32; i++) { modshl(8, a); a[0] += (spint)(unsigned char)b[i]; } res = (int)modfsb(a); nres(a, a); return res; }
static int modsign(const spint *a) { spint c[5]; redc(a, c); return c[0] % 2; }
static int modcmp(const spint *a, const spint *b) { spint c[5], d[5]; int eq = 1; redc(a, c); redc(b, d); for (int i = 0; i < 5; i++) eq &= (((c[i] ^ d[i]) - 1) >> 51) & 1; return eq; }
