#ifndef UTIL_32_H
#define UTIL_32_H

#include <stdint.h>

/*
 * x86/x86-64 GCC/Clang/MSVC:
 */
#if ((defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) ||          \
      defined(_M_X64)) &&                                                      \
     (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)))

#include <immintrin.h>

static inline unsigned char addcarry_u32(unsigned char c, uint32_t a,
                                         uint32_t b, uint32_t *d) {
  return _addcarry_u32(c, a, b, d);
}

static inline unsigned char subborrow_u32(unsigned char c, uint32_t a,
                                          uint32_t b, uint32_t *d) {
  return _subborrow_u32(c, a, b, d);
}

#else

/*
 * Portable fallback, this is all much simpler as we don't have to worry
 * about whether or not we have u128 types.
 */
static inline unsigned char addcarry_u32(unsigned char c, uint32_t a,
                                         uint32_t b, uint32_t *d) {
  uint64_t t = (uint64_t)a + (uint64_t)b + (uint64_t)c;

  *d = (uint32_t)t;
  return (unsigned char)(t >> 32);
}

static inline unsigned char subborrow_u32(unsigned char c, uint32_t a,
                                          uint32_t b, uint32_t *d) {
  uint64_t t = (uint64_t)a - (uint64_t)b - (uint64_t)c;

  *d = (uint32_t)t;
  return (unsigned char)((t >> 32) & 1);
}

#endif

static inline void mul_u32(uint32_t *lo, uint32_t *hi, uint32_t x, uint32_t y) {
  uint64_t t = (uint64_t)x * (uint64_t)y;

  *lo = (uint32_t)t;
  *hi = (uint32_t)(t >> 32);
}

static inline void mul_add(uint32_t *lo, uint32_t *hi, uint32_t x, uint32_t y,
                           uint32_t z) {
  uint64_t t = (uint64_t)x * (uint64_t)y + (uint64_t)z;

  *lo = (uint32_t)t;
  *hi = (uint32_t)(t >> 32);
}

static inline void mul_add2(uint32_t *lo, uint32_t *hi, uint32_t x, uint32_t y,
                            uint32_t z1, uint32_t z2) {
  uint64_t t = (uint64_t)x * (uint64_t)y + (uint64_t)z1 + (uint64_t)z2;

  *lo = (uint32_t)t;
  *hi = (uint32_t)(t >> 32);
}

static inline void mul_x2(uint32_t *lo, uint32_t *hi, uint32_t x1, uint32_t y1,
                          uint32_t x2, uint32_t y2) {
  uint32_t lo1, hi1;
  uint32_t lo2, hi2;

  mul_u32(&lo1, &hi1, x1, y1);
  mul_u32(&lo2, &hi2, x2, y2);

  unsigned char c = addcarry_u32(0, lo1, lo2, &lo1);

  (void)addcarry_u32(c, hi1, hi2, &hi1);

  *lo = lo1;
  *hi = hi1;
}

static inline void mul_x2_add(uint32_t *lo, uint32_t *hi, uint32_t x1,
                              uint32_t y1, uint32_t x2, uint32_t y2,
                              uint32_t z) {
  uint32_t tlo, thi;

  mul_x2(&tlo, &thi, x1, y1, x2, y2);

  unsigned char c = addcarry_u32(0, tlo, z, &tlo);

  (void)addcarry_u32(c, thi, 0, &thi);

  *lo = tlo;
  *hi = thi;
}

static inline uint32_t sign_word(uint32_t x) {
  return (uint32_t)(((int32_t)x) >> 31);
}

#endif /* UTIL_32_H */
