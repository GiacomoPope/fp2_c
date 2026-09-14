#!/usr/bin/env python3
import argparse
from pathlib import Path

MASK64 = (1 << 64) - 1


class FieldGenerator:
    def __init__(self, p: int):
        if p <= 3:
            raise ValueError("prime must be > 3")
        if p % 4 != 3:
            raise ValueError("this generator currently requires p == 3 (mod 4)")

        self.p = p
        self.bits = p.bit_length()
        self.n = (self.bits + 63) // 64

        if 2 * p - 4 >= (1 << (64 * self.n)):
            raise ValueError("sum/difference of products requires 2*p - 4 to fit in N limbs")

        self.encoded_length = (self.bits + 7) // 8
        self.modulus = [(p >> (64 * i)) & MASK64 for i in range(self.n)]

        self.r = pow(2, 64 * self.n, p)
        self.r2 = pow(2, 128 * self.n, p)
        self.clen = self.encoded_length - 1 if self.n == 1 else 8 * (self.n - 1)
        self.tdec = pow(2, 64 * self.n + 8 * self.clen, p)
        self.sqrt_exp = (p + 1) // 4        
        self.p_minus_three_div_four = (p - 3) // 4
        self.num1 = 0 if self.bits <= 32 else (2 * self.bits - 34) // 31
        remaining = max(2 * self.bits - 31 * self.num1, 0)
        self.num2 = remaining - 2 if remaining >= 2 else 0
        tfix_exp = self.num1 * 33 + 64 - self.num2 + 128 * self.n
        self.tfixdiv = pow(2, tfix_exp, p)
        self.p0i = (-pow(self.modulus[0], -1, 1 << 64)) & MASK64

        if self.bits < 32:
            self.p1 = p
        else:
            bl = self.bits & 63
            hi = self.modulus[-1]
            if bl == 0:
                self.p1 = hi >> 32
            elif bl < 32:
                lo = self.modulus[-2]
                self.p1 = ((hi << (32 - bl)) | (lo >> (bl + 32))) & MASK64
            else:
                self.p1 = hi >> (bl - 32)
        self.p1div_m = 1 + (((((1 << 32) - self.p1) << 64) // self.p1))
        self.p1div_m &= MASK64

    @staticmethod
    def limbs(x: int, n: int):
        return [(x >> (64 * i)) & MASK64 for i in range(n)]

    @staticmethod
    def c_u64(x: int) -> str:
        return f"UINT64_C(0x{x & MASK64:016x})"

    def array_literal(self, xs):
        return "{ " + ", ".join(self.c_u64(x) for x in xs) + " }"

    def generate_defs_header(self) -> str:
        return f"""\
#ifndef FP_DEFS_H
#define FP_DEFS_H

#define FP_LIMBS {self.n}
#define FP_BITS {self.bits}
#define FP_ENCODED_BYTES {self.encoded_length}
#define FP_DECODE_REDUCE_CHUNK {self.clen}

#endif /* FP_DEFS_H */
"""

    def generate_constants(self) -> str:
        return f"""\
static const uint64_t FP_MODULUS[FP_LIMBS] = {self.array_literal(self.modulus)};
static const uint64_t FP_P0I = {self.c_u64(self.p0i)};
static const uint64_t FP_P1 = {self.c_u64(self.p1)};
static const uint64_t FP_P1DIV_M = {self.c_u64(self.p1div_m)};
static const fp_t FP_ONE = {{ .limb = {self.array_literal(self.limbs(self.r, self.n))} }};
static const fp_t FP_R2_ELEM = {{ .limb = {self.array_literal(self.limbs(self.r2, self.n))} }};
static const fp_t FP_TDEC_ELEM = {{ .limb = {self.array_literal(self.limbs(self.tdec, self.n))} }};
static const uint64_t FP_SQRT_EXP[FP_LIMBS] = {self.array_literal(self.limbs(self.sqrt_exp, self.n))};
static const uint64_t FP_P_MINUS_3_DIV_FOUR[FP_LIMBS] = {self.array_literal(self.limbs(self.p_minus_three_div_four, self.n))};
static const size_t FP_NUM1 = {self.num1};
static const size_t FP_NUM2 = {self.num2};
static const fp_t FP_TFIXDIV = {{ .limb = {self.array_literal(self.limbs(self.tfixdiv, self.n))} }};
"""

    def generate_set_zero(self) -> str:
        return """\
inline void
fp_set_zero(fp_t *x)
{
    for (size_t i = 0; i < FP_LIMBS; i++)
        x->limb[i] = 0;
}
"""

    def generate_set_one(self) -> str:
        return """\
inline void
fp_set_one(fp_t *x)
{
    *x = FP_ONE;
}
"""

    def generate_set_small(self) -> str:
        return """\
inline void
fp_set_small(fp_t *x, const int32_t val)
{
    fp_mul_small(x, &FP_ONE, val);
}
"""

    def generate_copy(self) -> str:
        return """\
inline void
fp_copy(fp_t *a, const fp_t *b)
{
    *a = *b;
}
"""

    def generate_add(self) -> str:
        return """\
inline void
fp_add(fp_t *r, const fp_t *a, const fp_t *b)
{
    fp_t t;
    unsigned char c1 = 0;
    unsigned char c2 = 0;

    for (size_t i = 0; i < FP_LIMBS; i++) {
        c1 = addcarry_u64(c1, a->limb[i], b->limb[i],
                                  &t.limb[i]);
    }

    for (size_t i = 0; i < FP_LIMBS; i++) {
        c2 = subborrow_u64(c2, t.limb[i],
                                   FP_MODULUS[i], &t.limb[i]);
    }

    uint64_t mask = (uint64_t)c1 - (uint64_t)c2;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        carry = addcarry_u64(carry, t.limb[i],
                                     FP_MODULUS[i] & mask, &t.limb[i]);
    }

    *r = t;
}
"""

    def generate_sub(self) -> str:
        return """\
inline void
fp_sub(fp_t *r, const fp_t *a, const fp_t *b)
{
    fp_t t;
    unsigned char borrow = 0;

    for (size_t i = 0; i < FP_LIMBS; i++) {
        borrow = subborrow_u64(borrow, a->limb[i],
                                      b->limb[i], &t.limb[i]);
    }

    uint64_t mask = (uint64_t)0 - borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        carry = addcarry_u64(carry, t.limb[i],
                                     FP_MODULUS[i] & mask, &t.limb[i]);
    }

    *r = t;
}
"""

    def generate_neg(self) -> str:
        return """\
inline void
fp_neg(fp_t *r, const fp_t *a)
{
    fp_t t;
    unsigned char borrow = 0;

    for (size_t i = 0; i < FP_LIMBS; i++) {
        borrow = subborrow_u64(borrow, 0, a->limb[i], &t.limb[i]);
    }

    uint64_t mask = (uint64_t)0 - borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        carry = addcarry_u64(carry, t.limb[i],
                                     FP_MODULUS[i] & mask, &t.limb[i]);
    }

    *r = t;
}
"""

    def generate_double(self) -> str:
        return """\
inline void
fp_double(fp_t *out, const fp_t *a)
{
    fp_t t = *a;
    unsigned char cc = 0;
    unsigned char tb = 0;

    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint64_t w = t.limb[i];
        uint64_t d = (w << 1) | tb;
        tb = (unsigned char)(w >> 63);
        cc = subborrow_u64(cc, d, FP_MODULUS[i], &t.limb[i]);
    }

    uint64_t mm = (uint64_t)tb - (uint64_t)cc;
    cc = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        cc = addcarry_u64(cc, t.limb[i],
                                  mm & FP_MODULUS[i], &t.limb[i]);

    *out = t;
}
"""

    def generate_half(self) -> str:
        return """\
inline void
fp_half(fp_t *out, const fp_t *a)
{
    fp_t t = *a;
    uint64_t m = (uint64_t)0 - (t.limb[0] & 1u);
    unsigned char carry = 0;

    for (size_t i = 0; i < FP_LIMBS; i++)
        carry = addcarry_u64(carry, t.limb[i],
                                     FP_MODULUS[i] & m, &t.limb[i]);

    for (size_t i = 0; i + 1 < FP_LIMBS; i++)
        t.limb[i] = (t.limb[i] >> 1) | (t.limb[i + 1] << 63);
    t.limb[FP_LIMBS - 1] = (t.limb[FP_LIMBS - 1] >> 1) |
                           ((uint64_t)carry << 63);
    *out = t;
}
"""

    def generate_equals(self) -> str:
        return """\
inline uint32_t
fp_equals(const fp_t *a, const fp_t *b)
{
    uint64_t r = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        r |= a->limb[i] ^ b->limb[i];

    return (uint32_t)(((r | (uint64_t)(0 - r)) >> 63) - 1);
}
"""

    def generate_is_zero(self) -> str:
        return """\
inline uint32_t
fp_is_zero(const fp_t *a)
{
    uint64_t x = a->limb[0];

    for (size_t i = 1; i < FP_LIMBS; i++) {
        x |= a->limb[i];
    }

    x |= (uint64_t)(0 - x);

    uint64_t s = (uint64_t)(((int64_t)x) >> 63);
    return (uint32_t)(~s);
}
"""

    def generate_ct_helpers(self) -> str:
        return """\
inline void
fp_select(fp_t *out, const fp_t *a, const fp_t *b, uint32_t ctl)
{
    uint64_t c = (uint64_t)ctl | ((uint64_t)ctl << 32);
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint64_t wa = a->limb[i];
        uint64_t wb = b->limb[i];
        out->limb[i] = wa ^ (c & (wa ^ wb));
    }
}

inline void
fp_cond_swap(fp_t *a, fp_t *b, uint32_t ctl)
{
    uint64_t c = (uint64_t)ctl | ((uint64_t)ctl << 32);
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint64_t wa = a->limb[i];
        uint64_t wb = b->limb[i];
        uint64_t wc = c & (wa ^ wb);
        a->limb[i] = wa ^ wc;
        b->limb[i] = wb ^ wc;
    }
}

inline void
fp_cond_neg(fp_t *a, uint32_t ctl)
{
    fp_t neg;
    fp_neg(&neg, a);
    fp_select(a, a, &neg, ctl);
}
"""


    def generate_reduce(self) -> str:
        return """\
static inline void
fp_internal_reduce(fp_t *x)
{
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint64_t f = x->limb[0] * FP_P0I;
        uint64_t lo, cc;

        mul_add(&lo, &cc,
                        f, FP_MODULUS[0], x->limb[0]);

        for (size_t j = 1; j < FP_LIMBS; j++) {
            uint64_t hi;
            mul_add2(&lo, &hi,
                             f, FP_MODULUS[j],
                             x->limb[j], cc);
            x->limb[j - 1] = lo;
            cc = hi;
        }

        x->limb[FP_LIMBS - 1] = cc;
    }
}
"""

    def generate_mul_small_n(self) -> str:
        N = self.n

        lines = [
            "inline void",
            "fp_mul(fp_t *out, const fp_t *a, const fp_t *b)",
            "{",
            "    fp_t t = { { 0 } };",
            "    unsigned char cch = 0;",
            "    uint64_t lo, hi;",
            "",
        ]

        for i in range(N):
            lines.append(f"    /* i = {i} */")
            lines.append("    {")
            lines.append("        uint64_t cc1;")
            lines.append(
                f"        mul_add(&lo, &cc1, b->limb[{i}], a->limb[0], t.limb[0]);"
            )
            lines.append("        t.limb[0] = lo;")

            for j in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, b->limb[{i}], a->limb[{j}], "
                    f"t.limb[{j}], cc1);"
                )
                lines.append(f"        t.limb[{j}] = lo;")
                lines.append("        cc1 = hi;")

            lines.append("")
            lines.append("        uint64_t q = t.limb[0] * FP_P0I;")
            lines.append("        uint64_t cc2;")
            lines.append(
                "        mul_add(&lo, &cc2, q, FP_MODULUS[0], t.limb[0]);"
            )

            for j in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, q, FP_MODULUS[{j}], "
                    f"t.limb[{j}], cc2);"
                )
                lines.append(f"        t.limb[{j - 1}] = lo;")
                lines.append("        cc2 = hi;")

            lines.append("")
            lines.append("        uint64_t top;")
            lines.append("        unsigned char carry = addcarry_u64(0, cc1, cc2, &top);")
            lines.append("        carry = addcarry_u64(carry, top, cch, &top);")
            lines.append(f"        t.limb[{N - 1}] = top;")
            lines.append("        cch = carry;")
            lines.append("    }")
            lines.append("")

        lines.append("    unsigned char borrow = 0;")
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u64(borrow, t.limb[{i}], "
                f"FP_MODULUS[{i}], &t.limb[{i}]);"
            )

        lines.append("")
        lines.append("    uint64_t mask = (uint64_t)cch - (uint64_t)borrow;")
        lines.append("    unsigned char carry = 0;")
        for i in range(N):
            lines.append(
                f"    carry = addcarry_u64(carry, t.limb[{i}], "
                f"FP_MODULUS[{i}] & mask, &t.limb[{i}]);"
            )

        lines.append("")
        lines.append("    *out = t;")
        lines.append("}\n")

        return "\n".join(lines)

    def generate_mul_large_n(self) -> str:
        N = self.n

        lines = [
            "inline void",
            "fp_mul(fp_t *out, const fp_t *a, const fp_t *b)",
            "{",
            "    fp_t t = { { 0 } };",
            "    unsigned char cch = 0;",
            "    uint64_t lo, hi;",
            "",
        ]

        for i in range(N):
            lines.append(f"    /* i = {i} */")
            lines.append("    {")

            lines.append(f"        uint64_t f = b->limb[{i}];")
            lines.append("        uint64_t cc1;")
            lines.append("        uint64_t cc2;")
            lines.append("        uint64_t d;")

            # First multiplication:
            # lo = f * a[0] + t[0]
            lines.append(
                "        mul_add(&lo, &cc1, "
                "f, a->limb[0], t.limb[0]);"
            )

            # Montgomery factor.
            lines.append(
                "        uint64_t g = lo * FP_P0I;"
            )

            # First reduction limb:
            # lo = g * p[0] + lo
            lines.append(
                "        mul_add(&lo, &cc2, "
                "g, FP_MODULUS[0], lo);"
            )

            for j in range(1, N):
                # Multiplication.
                lines.append(
                    f"        mul_add2(&d, &hi, "
                    f"f, a->limb[{j}], t.limb[{j}], cc1);"
                )
                lines.append("        cc1 = hi;")

                # Montgomery reduction.
                lines.append(
                    f"        mul_add2(&d, &hi, "
                    f"g, FP_MODULUS[{j}], d, cc2);"
                )
                lines.append("        cc2 = hi;")

                # Shift result down one limb.
                lines.append(
                    f"        t.limb[{j - 1}] = d;"
                )

            lines.append("")

            # Carry into the top limb.
            lines.append("        uint64_t top;")
            lines.append(
                "        unsigned char carry = "
                "addcarry_u64(0, cc1, cc2, &top);"
            )
            lines.append(
                "        carry = "
                "addcarry_u64(carry, top, cch, &top);"
            )
            lines.append(
                f"        t.limb[{N - 1}] = top;"
            )
            lines.append("        cch = carry;")

            lines.append("    }")
            lines.append("")

        # Final reduction.
        lines.append("    unsigned char borrow = 0;")

        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u64("
                f"borrow, t.limb[{i}], "
                f"FP_MODULUS[{i}], &t.limb[{i}]);"
            )

        lines.append("")

        lines.append(
            "    uint64_t mask = "
            "(uint64_t)cch - (uint64_t)borrow;"
        )

        lines.append("    unsigned char carry = 0;")

        for i in range(N):
            lines.append(
                f"    carry = addcarry_u64("
                f"carry, t.limb[{i}], "
                f"FP_MODULUS[{i}] & mask, "
                f"&t.limb[{i}]);"
            )

        lines.extend([
            "",
            "    *out = t;",
            "}\n",
        ])

        return "\n".join(lines)

    def generate_mul(self) -> str:
        if self.n >= 15:
            return self.generate_mul_large_n()
        return self.generate_mul_small_n()

    def generate_sqr(self) -> str:
        N = self.n

        lines = [
            "inline void",
            "fp_sqr(fp_t *out, const fp_t *a)",
            "{",
            "    uint64_t t[FP_LIMBS * 2] = { 0 };",
            "    uint64_t f, lo, hi, w, ee, cc;",
            "",
            "    /* --- upper triangle, row 0 --- */",
            "    f = a->limb[0];",
            "    mul_u64(&t[1], &cc, f, a->limb[1]);",
        ]

        for j in range(2, N):
            lines.append(f"    mul_add(&t[{j}], &hi, f, a->limb[{j}], cc);")
            lines.append("    cc = hi;")
        lines.append(f"    t[{N}] = cc;")
        lines.append("")

        lines.append("    /* --- upper triangle, remaining rows --- */")
        for i in range(1, N - 1):
            lines.append(f"    /* i = {i} */")
            lines.append(f"    f = a->limb[{i}];")
            lines.append(
                f"    mul_add(&t[{2 * i + 1}], &cc, f, a->limb[{i + 1}], t[{2 * i + 1}]);"
            )
            for j in range(i + 2, N):
                lines.append(
                    f"    mul_add2(&t[{i + j}], &hi, f, a->limb[{j}], t[{i + j}], cc);"
                )
                lines.append("    cc = hi;")
            lines.append(f"    t[{i + N}] = cc;")
        lines.append("")

        lines.append("    /* --- double the cross-product terms --- */")
        lines.append("    cc = 0;")
        for i in range(1, 2 * N - 1):
            lines.append(f"    w = t[{i}];")
            lines.append("    hi = w >> 63;")
            lines.append(f"    t[{i}] = (w << 1) | cc;")
            lines.append("    cc = hi;")
        lines.append(f"    t[{2 * N - 1}] = cc;")
        lines.append("")

        lines.append("    /* --- add in the diagonal squares --- */")
        lines.append("    cc = 0;")
        for i in range(N):
            lines.append(f"    mul_u64(&lo, &hi, a->limb[{i}], a->limb[{i}]);")
            lines.append(
                f"    ee = addcarry_u64((unsigned char)cc, lo, t[{2 * i}], &t[{2 * i}]);"
            )
            lines.append(
                f"    ee = addcarry_u64(ee, hi, t[{2 * i + 1}], &t[{2 * i + 1}]);"
            )
            lines.append("    cc = ee;")
        lines.append("")

        lines.append("    fp_t fp_lo = { { 0 } };")
        lines.append("    fp_t fp_hi = { { 0 } };")
        for i in range(N):
            lines.append(f"    fp_lo.limb[{i}] = t[{i}];")
            lines.append(f"    fp_hi.limb[{i}] = t[{i + N}];")
        lines.append("    fp_internal_reduce(&fp_lo);")
        lines.append("    fp_add(out, &fp_lo, &fp_hi);")
        lines.append("}")

        return "\n".join(lines)

    def generate_n_sqr(self) -> str:
        return r"""\
void
fp_n_sqr(fp_t *out, const fp_t *a, uint32_t n)
{
    fp_t t = *a;
    for (uint32_t i = 0; i < n; i++)
        fp_sqr(&t, &t);
    *out = t;
}
"""

    def generate_mul_small(self) -> str:
        bl = self.bits & 63
        if bl == 0:
            x1_expr = "x1 = (t.limb[FP_LIMBS - 1] >> 32) | (hi << 32);"
        elif bl < 32:
            x1_expr = (
                "x1 = (t.limb[FP_LIMBS - 1] << %d) | "
                "(t.limb[FP_LIMBS - 2] >> %d);"
            ) % (32 - bl, 32 + bl)
        elif bl == 32:
            x1_expr = "x1 = t.limb[FP_LIMBS - 1];"
        else:
            x1_expr = (
                "x1 = (hi << %d) | "
                "(t.limb[FP_LIMBS - 1] >> %d);"
            ) % (96 - bl, bl - 32)
        return r"""
inline void
fp_mul_small(fp_t *out, const fp_t *a, int32_t k)
{
    fp_t t = *a;

    /* Get the absolute value of k, while retaining its sign. */
    uint32_t sk = (uint32_t)((int64_t)k >> 31);
    uint32_t ak = (((uint32_t)k ^ sk) - sk);

    /* Compute the product over integers. */
    uint64_t lo, hi;
    mul_u64(&lo, &hi, t.limb[0], (uint64_t)ak);
    t.limb[0] = lo;
    for (size_t i = 1; i < FP_LIMBS; i++) {
        uint64_t d, ee;
        mul_add(&d, &ee, t.limb[i], (uint64_t)ak, hi);
        t.limb[i] = d;
        hi = ee;
    }

    /* Extract the top word of the unreduced product. */
    uint64_t x1;
    /* The shift pattern is selected at generation time to avoid invalid
       compile-time shift counts in other branches. */
    __X1_EXPRESSION__

    /* Compute b = floor(x1 / P1) with the Granlund-Montgomery reciprocal. */
    uint64_t ignored, reciprocal;
    mul_u64(&ignored, &reciprocal, x1, FP_P1DIV_M);
    uint64_t bq = ((x1 - reciprocal) >> 1) + reciprocal;
    bq >>= 31;
    bq += (FP_P1 - bq) >> 63;

    /* Subtract bq * p from x. */
    uint64_t cc1 = 0;
    unsigned char cc2 = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint64_t d, ee;
        mul_add(&d, &ee, bq, FP_MODULUS[i], cc1);
        cc1 = ee;
        cc2 = subborrow_u64(cc2, t.limb[i], d, &t.limb[i]);
    }
    {
        uint64_t d;
        (void)subborrow_u64(cc2, hi, cc1, &d);
        hi = d;
    }

    /* Add p (at most twice) while the result is negative. */
    for (unsigned i = 0; i < 2; i++) {
        uint64_t m = sign_word(hi);
        unsigned char carry = 0;
        for (size_t j = 0; j < FP_LIMBS; j++)
            carry = addcarry_u64(carry, t.limb[j],
                                         m & FP_MODULUS[j], &t.limb[j]);
        hi += carry;
    }

    *out = t;
    fp_cond_neg(out, sk);
}
""".replace("__X1_EXPRESSION__", x1_expr)

    
    def generate_sum_of_products(self) -> str:
        N = self.n

        lines = [
            "inline void",
            "fp_sum_of_products(fp_t *out, const fp_t *a1, const fp_t *b1,",
            "                   const fp_t *a2, const fp_t *b2)",
            "{",
            "    fp_t u = { { 0 } };",
            "    unsigned char cch = 0;",
            "    uint64_t lo, hi;",
            "",
        ]

        for j in range(N):
            lines.append(f"    /* --- j = {j} --- */")
            lines.append("    {")
            lines.append("        uint64_t cc1, cc2, cc3;")
            lines.append(
                f"        mul_add(&lo, &cc1, a1->limb[{j}], b1->limb[0], u.limb[0]);"
            )
            lines.append("        u.limb[0] = lo;")
            for k in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, a1->limb[{j}], b1->limb[{k}], "
                    f"u.limb[{k}], cc1);"
                )
                lines.append(f"        u.limb[{k}] = lo;")
                lines.append("        cc1 = hi;")

            lines.append(
                f"        mul_add(&lo, &cc2, a2->limb[{j}], b2->limb[0], u.limb[0]);"
            )
            lines.append("        u.limb[0] = lo;")
            for k in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, a2->limb[{j}], b2->limb[{k}], "
                    f"u.limb[{k}], cc2);"
                )
                lines.append(f"        u.limb[{k}] = lo;")
                lines.append("        cc2 = hi;")

            lines.append("")
            lines.append("        uint64_t q = u.limb[0] * FP_P0I;")
            lines.append(
                "        mul_add(&lo, &cc3, q, FP_MODULUS[0], u.limb[0]);"
            )
            for k in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, q, FP_MODULUS[{k}], "
                    f"u.limb[{k}], cc3);"
                )
                lines.append(f"        u.limb[{k - 1}] = lo;")
                lines.append("        cc3 = hi;")

            lines.append("")
            lines.append("        uint64_t top;")
            lines.append(
                "        unsigned char c1 = addcarry_u64(cch, cc1, cc2, &top);"
            )
            lines.append(
                "        unsigned char c2 = addcarry_u64(0, top, cc3, &top);"
            )
            lines.append("        cch = (unsigned char)(c1 + c2);")
            lines.append(f"        u.limb[{N - 1}] = top;")
            lines.append("    }")
            lines.append("")

        lines.append("    unsigned char borrow = 0;")
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u64(borrow, u.limb[{i}], "
                f"FP_MODULUS[{i}], &u.limb[{i}]);"
            )
        lines.append("")
        lines.append("    uint64_t mask = (uint64_t)cch - (uint64_t)borrow;")
        lines.append("    unsigned char carry = 0;")
        for i in range(N):
            lines.append(
                f"    carry = addcarry_u64(carry, u.limb[{i}], "
                f"FP_MODULUS[{i}] & mask, &u.limb[{i}]);"
            )
        lines.append("    (void)carry;")
        lines.append("    *out = u;")
        lines.append("}")

        return "\n".join(lines)

    def generate_difference_of_products(self) -> str:
        return """\
inline void
fp_difference_of_products(fp_t *out, const fp_t *a1, const fp_t *b1,
                          const fp_t *a2, const fp_t *b2)
{
    fp_t nb2 = { { 0 } };
    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        borrow = subborrow_u64(borrow, FP_MODULUS[i], b2->limb[i], &nb2.limb[i]);
    (void)borrow;
    fp_sum_of_products(out, a1, b1, a2, &nb2);
}
"""




    def generate_binary_gcd_helpers(self) -> str:
        return """\
static inline void
fp_montylin(fp_t *out, const fp_t *u, const fp_t *v, uint64_t f, uint64_t g)
{
    uint64_t sf = sign_word(f);
    uint64_t af = (f ^ sf) - sf;
    uint64_t sg = sign_word(g);
    uint64_t ag = (g ^ sg) - sg;
    fp_t tu = *u, tv = *v, neg;
    fp_neg(&neg, u);
    for (size_t i = 0; i < FP_LIMBS; i++)
        tu.limb[i] ^= sf & (tu.limb[i] ^ neg.limb[i]);
    fp_neg(&neg, v);
    for (size_t i = 0; i < FP_LIMBS; i++)
        tv.limb[i] ^= sg & (tv.limb[i] ^ neg.limb[i]);

    uint64_t lo, cc;
    mul_x2(&lo, &cc, tu.limb[0], af, tv.limb[0], ag);
    out->limb[0] = lo;
    for (size_t i = 1; i < FP_LIMBS; i++) {
        uint64_t hi;
        mul_x2_add(&lo, &hi, tu.limb[i], af, tv.limb[i], ag, cc);
        out->limb[i] = lo;
        cc = hi;
    }
    uint64_t up = cc;

    uint64_t q = out->limb[0] * FP_P0I;
    uint64_t cc2;
    mul_add(&lo, &cc2, q, FP_MODULUS[0], out->limb[0]);
    for (size_t i = 1; i < FP_LIMBS; i++) {
        uint64_t hi;
        mul_add2(&lo, &hi, q, FP_MODULUS[i], out->limb[i], cc2);
        out->limb[i - 1] = lo;
        cc2 = hi;
    }
    unsigned char cc1 = addcarry_u64(0, up, cc2,
                                               &out->limb[FP_LIMBS - 1]);

    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        borrow = subborrow_u64(borrow, out->limb[i], FP_MODULUS[i], &out->limb[i]);

    uint64_t mask = (uint64_t)cc1 - (uint64_t)borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        carry = addcarry_u64(carry, out->limb[i], FP_MODULUS[i] & mask, &out->limb[i]);
    (void)carry;
}

static inline uint64_t
fp_lindiv31abs(fp_t *out, const fp_t *a, const fp_t *b, uint64_t f, uint64_t g)
{
    uint64_t sf = sign_word(f);
    uint64_t af = (f ^ sf) - sf;
    uint64_t sg = sign_word(g);
    uint64_t ag = (g ^ sg) - sg;
    unsigned char cc1 = 0, cc2 = 0;
    uint64_t cc3 = 0;

    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint64_t aa, bb, d, hi;
        cc1 = subborrow_u64(cc1, a->limb[i] ^ sf, sf, &aa);
        cc2 = subborrow_u64(cc2, b->limb[i] ^ sg, sg, &bb);
        mul_x2_add(&d, &hi, aa, af, bb, ag, cc3);
        out->limb[i] = d;
        cc3 = hi;
    }

    uint64_t up = cc3 - (((uint64_t)0 - (uint64_t)cc1) & af)
                    - (((uint64_t)0 - (uint64_t)cc2) & ag);
    for (size_t i = 0; i + 1 < FP_LIMBS; i++)
        out->limb[i] = (out->limb[i] >> 31) | (out->limb[i + 1] << 33);
    out->limb[FP_LIMBS - 1] = (out->limb[FP_LIMBS - 1] >> 31) | (up << 33);

    uint64_t w = sign_word(up);
    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        borrow = subborrow_u64(borrow, out->limb[i] ^ w, w, &out->limb[i]);
    (void)borrow;
    return w;
}
"""

    def generate_legendre(self) -> str:
        return """\
int32_t
fp_legendre(const fp_t *x)
{
    /*
     * Same optimized binary GCD as inversion, but without the Bezout
     * coefficients. We can operate directly on Montgomery values because
     * R = 2^(64*N) is a square in GF(p).
     */
    fp_t a = *x;
    fp_t b = { { 0 } };
    for (size_t i = 0; i < FP_LIMBS; i++)
        b.limb[i] = FP_MODULUS[i];
    uint64_t ls = 0;

    for (size_t outer = 0; outer < FP_NUM1; outer++) {
        uint64_t c_hi = UINT64_MAX, c_lo = UINT64_MAX;
        uint64_t a_hi = 0, a_lo = 0, b_hi = 0, b_lo = 0;

        for (size_t j = FP_LIMBS; j-- > 0;) {
            uint64_t aw = a.limb[j], bw = b.limb[j];
            a_hi ^= (a_hi ^ aw) & c_hi;
            a_lo ^= (a_lo ^ aw) & c_lo;
            b_hi ^= (b_hi ^ bw) & c_hi;
            b_lo ^= (b_lo ^ bw) & c_lo;
            c_lo = c_hi;
            uint64_t mw = aw | bw;
            c_hi &= ((mw | (uint64_t)(0 - mw)) >> 63) - 1;
        }

        unsigned s = (unsigned)__builtin_clzll(a_hi | b_hi);
        uint64_t xa = (a_hi << s) | ((a_lo >> 1) >> (63 - s));
        uint64_t xb = (b_hi << s) | ((b_lo >> 1) >> (63 - s));
        xa = (xa & UINT64_C(0xffffffff80000000)) |
             (a.limb[0] & UINT64_C(0x7fffffff));
        xb = (xb & UINT64_C(0xffffffff80000000)) |
             (b.limb[0] & UINT64_C(0x7fffffff));
        xa ^= c_lo & (xa ^ a.limb[0]);
        xb ^= c_lo & (xb ^ b.limb[0]);

        /* First 29 inner iterations. */
        uint64_t fg0 = 1, fg1 = UINT64_C(1) << 32;
        for (unsigned k = 0; k < 29; k++) {
            uint64_t a_odd = (uint64_t)0 - (xa & 1);
            uint64_t d;
            unsigned char borrow = subborrow_u64(0, xa, xb, &d);
            uint64_t swap = a_odd & ((uint64_t)0 - (uint64_t)borrow);

            ls ^= swap & ((xa & xb) >> 1);

            uint64_t t = swap & (xa ^ xb);
            xa ^= t;
            xb ^= t;
            t = swap & (fg0 ^ fg1);
            fg0 ^= t;
            fg1 ^= t;

            xa -= a_odd & xb;
            fg0 -= a_odd & fg1;
            xa >>= 1;
            fg1 <<= 1;
            ls ^= (xb + 2) >> 2;
        }

        /* Recover the low words needed to perform two more iterations. */
        uint64_t fg0z = fg0 + UINT64_C(0x7fffffff7fffffff);
        uint64_t fg1z = fg1 + UINT64_C(0x7fffffff7fffffff);
        uint64_t f0 = (fg0z & UINT64_C(0xffffffff)) - UINT64_C(0x7fffffff);
        uint64_t g0 = (fg0z >> 32) - UINT64_C(0x7fffffff);
        uint64_t f1 = (fg1z & UINT64_C(0xffffffff)) - UINT64_C(0x7fffffff);
        uint64_t g1 = (fg1z >> 32) - UINT64_C(0x7fffffff);
        uint64_t a0 = (a.limb[0] * f0 + b.limb[0] * g0) >> 29;
        uint64_t b0 = (a.limb[0] * f1 + b.limb[0] * g1) >> 29;

        for (unsigned k = 0; k < 2; k++) {
            uint64_t a_odd = (uint64_t)0 - (xa & 1);
            uint64_t d;
            unsigned char borrow = subborrow_u64(0, xa, xb, &d);
            uint64_t swap = a_odd & ((uint64_t)0 - (uint64_t)borrow);

            ls ^= swap & ((a0 & b0) >> 1);

            uint64_t t = swap & (xa ^ xb);
            xa ^= t;
            xb ^= t;
            t = swap & (fg0 ^ fg1);
            fg0 ^= t;
            fg1 ^= t;
            t = swap & (a0 ^ b0);
            a0 ^= t;
            b0 ^= t;

            xa -= a_odd & xb;
            fg0 -= a_odd & fg1;
            a0 -= a_odd & b0;
            xa >>= 1;
            fg1 <<= 1;
            a0 >>= 1;
            ls ^= (b0 + 2) >> 2;
        }

        /* Propagate the 31-step transformation to the full operands. */
        fg0 += UINT64_C(0x7fffffff7fffffff);
        fg1 += UINT64_C(0x7fffffff7fffffff);
        f0 = (fg0 & UINT64_C(0xffffffff)) - UINT64_C(0x7fffffff);
        g0 = (fg0 >> 32) - UINT64_C(0x7fffffff);
        f1 = (fg1 & UINT64_C(0xffffffff)) - UINT64_C(0x7fffffff);
        g1 = (fg1 >> 32) - UINT64_C(0x7fffffff);

        fp_t na, nb;
        uint64_t nega = fp_lindiv31abs(&na, &a, &b, f0, g0);
        (void)fp_lindiv31abs(&nb, &a, &b, f1, g1);
        ls ^= nega & (nb.limb[0] >> 1);
        a = na;
        b = nb;
    }

    /* The remaining operands fit in one word. */
    uint64_t xa = a.limb[0];
    uint64_t xb = b.limb[0];
    for (size_t k = 0; k < FP_NUM2; k++) {
        uint64_t a_odd = (uint64_t)0 - (xa & 1);
        uint64_t d;
        unsigned char borrow = subborrow_u64(0, xa, xb, &d);
        uint64_t swap = a_odd & ((uint64_t)0 - (uint64_t)borrow);

        ls ^= swap & ((xa & xb) >> 1);

        uint64_t t = swap & (xa ^ xb);
        xa ^= t;
        xb ^= t;
        xa -= a_odd & xb;
        xa >>= 1;
        ls ^= (xb + 2) >> 2;
    }

    /* 0 -> 0, QR -> +1, QNR -> -1. */
    uint32_t r = 1u - (((uint32_t)ls & 1u) << 1);
    uint64_t z = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        z |= x->limb[i];
    uint32_t nz = (uint32_t)((z | (uint64_t)(0 - z)) >> 63);
    r &= (uint32_t)0 - nz;
    return (int32_t)r;
}
"""

    def generate_is_square(self) -> str:
        return """\
uint32_t
fp_is_square(const fp_t *a) {
    int32_t val = fp_legendre(a);
    return -(uint32_t)(val & ~(val >> 31));
}
    """

    def generate_inv(self) -> str:
        return """\
void
fp_inv(fp_t *out, const fp_t *x)
{
    fp_t a = *x;
    fp_t b = { { 0 } };
    for (size_t i = 0; i < FP_LIMBS; i++)
        b.limb[i] = FP_MODULUS[i];
    fp_t u = FP_ONE;
    fp_t v = { { 0 } };

    for (size_t outer = 0; outer < FP_NUM1; outer++) {
        uint64_t c_hi = UINT64_MAX, c_lo = UINT64_MAX;
        uint64_t a_hi = 0, a_lo = 0, b_hi = 0, b_lo = 0;
        for (size_t j = FP_LIMBS; j-- > 0;) {
            uint64_t aw = a.limb[j], bw = b.limb[j];
            a_hi ^= (a_hi ^ aw) & c_hi;
            a_lo ^= (a_lo ^ aw) & c_lo;
            b_hi ^= (b_hi ^ bw) & c_hi;
            b_lo ^= (b_lo ^ bw) & c_lo;
            c_lo = c_hi;
            uint64_t mw = aw | bw;
            c_hi &= ((mw | (uint64_t)(0 - mw)) >> 63) - 1;
        }

        unsigned s = (unsigned)__builtin_clzll(a_hi | b_hi);
        uint64_t xa = (a_hi << s) | ((a_lo >> 1) >> (63 - s));
        uint64_t xb = (b_hi << s) | ((b_lo >> 1) >> (63 - s));
        xa = (xa & UINT64_C(0xffffffff80000000)) | (a.limb[0] & UINT64_C(0x7fffffff));
        xb = (xb & UINT64_C(0xffffffff80000000)) | (b.limb[0] & UINT64_C(0x7fffffff));
        xa ^= c_lo & (xa ^ a.limb[0]);
        xb ^= c_lo & (xb ^ b.limb[0]);

        uint64_t fg0 = 1, fg1 = UINT64_C(1) << 32;
        for (unsigned k = 0; k < 31; k++) {
            uint64_t a_odd = (uint64_t)0 - (xa & 1);
            uint64_t d;
            unsigned char borrow = subborrow_u64(0, xa, xb, &d);
            uint64_t swap = a_odd & ((uint64_t)0 - (uint64_t)borrow);
            uint64_t t = swap & (xa ^ xb); xa ^= t; xb ^= t;
            t = swap & (fg0 ^ fg1); fg0 ^= t; fg1 ^= t;
            xa -= a_odd & xb;
            fg0 -= a_odd & fg1;
            xa >>= 1;
            fg1 <<= 1;
        }
        fg0 += UINT64_C(0x7fffffff7fffffff);
        fg1 += UINT64_C(0x7fffffff7fffffff);
        uint64_t f0 = (fg0 & UINT64_C(0xffffffff)) - UINT64_C(0x7fffffff);
        uint64_t g0 = (fg0 >> 32) - UINT64_C(0x7fffffff);
        uint64_t f1 = (fg1 & UINT64_C(0xffffffff)) - UINT64_C(0x7fffffff);
        uint64_t g1 = (fg1 >> 32) - UINT64_C(0x7fffffff);

        fp_t na, nb, nu, nv;
        uint64_t nega = fp_lindiv31abs(&na, &a, &b, f0, g0);
        uint64_t negb = fp_lindiv31abs(&nb, &a, &b, f1, g1);
        f0 = (f0 ^ nega) - nega; g0 = (g0 ^ nega) - nega;
        f1 = (f1 ^ negb) - negb; g1 = (g1 ^ negb) - negb;
        fp_montylin(&nu, &u, &v, f0, g0);
        fp_montylin(&nv, &u, &v, f1, g1);
        a = na; b = nb; u = nu; v = nv;
    }

    uint64_t xa = a.limb[0], xb = b.limb[0];
    uint64_t f0 = 1, g0 = 0, f1 = 0, g1 = 1;
    for (size_t k = 0; k < FP_NUM2; k++) {
        uint64_t a_odd = (uint64_t)0 - (xa & 1);
        uint64_t d;
        unsigned char borrow = subborrow_u64(0, xa, xb, &d);
        uint64_t swap = a_odd & ((uint64_t)0 - (uint64_t)borrow);
        uint64_t t = swap & (xa ^ xb); xa ^= t; xb ^= t;
        t = swap & (f0 ^ f1); f0 ^= t; f1 ^= t;
        t = swap & (g0 ^ g1); g0 ^= t; g1 ^= t;
        xa -= a_odd & xb;
        f0 -= a_odd & f1;
        g0 -= a_odd & g1;
        xa >>= 1;
        f1 <<= 1;
        g1 <<= 1;
    }

    fp_montylin(out, &u, &v, f1, g1);

    uint64_t z = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) z |= x->limb[i];
    uint64_t mask = (uint64_t)0 - ((z | (uint64_t)(0 - z)) >> 63);
    for (size_t i = 0; i < FP_LIMBS; i++) out->limb[i] &= mask;
    fp_mul(out, out, &FP_TFIXDIV);
}
"""

    def generate_vartime_pow(self) -> str:
        return """\
void
fp_pow_pubexp(fp_t *out, const fp_t *a, const uint64_t e[FP_LIMBS])
{
    fp_t win[15];
    win[0] = *a;
    for (size_t i = 1; i < 8; i++) {
        size_t j = i * 2;
        fp_sqr(&win[j - 1], &win[i - 1]);
        fp_mul(&win[j], &win[j - 1], &win[0]);
    }

    fp_t t = { { 0 } };
    int started = 0;
    for (size_t i = FP_LIMBS; i-- > 0;) {
        uint64_t ew = e[i];
        for (unsigned j = 16; j-- > 0;) {
            unsigned c = (unsigned)((ew >> (j * 4)) & 0x0f);
            if (started)
                fp_n_sqr(&t, &t, 4);
            if (c != 0) {
                if (started)
                    fp_mul(&t, &t, &win[c - 1]);
                else {
                    t = win[c - 1];
                    started = 1;
                }
            }
        }
    }
    if (!started)
        t = FP_ONE;
    *out = t;
}
"""

    def generate_sqrt(self) -> str:
        return """\
uint32_t
fp_sqrt(fp_t *out, const fp_t *a)
{
    fp_t y;
    fp_pow_pubexp(&y, a, FP_SQRT_EXP);

    fp_t check;
    fp_sqr(&check, &y);
    uint32_t ok = fp_equals(&check, a);
    uint64_t okmask = (uint64_t)ok | ((uint64_t)ok << 32);
    for (size_t i = 0; i < FP_LIMBS; i++)
        y.limb[i] &= okmask;

    uint8_t enc[FP_ENCODED_BYTES];
    fp_encode(enc, &y);
    fp_cond_neg(&y, (uint32_t)0 - (uint32_t)(enc[0] & 1u));
    *out = y;
    return ok;
}
"""

    def generate_exp3div4(self) -> str:
        return """\
void
fp_exp3div4(fp_t *out, const fp_t *a)
{
    fp_pow_pubexp(out, a, FP_P_MINUS_3_DIV_FOUR);
}
"""

    def generate_batch_inversion(self) -> str:
        return """\
void
fp_batch_invert(fp_t *x, size_t len) {
    size_t i = 0;
    while (i < len) {
        size_t blen = len - i;
        if (blen > 200)
            blen = 200;

        fp_t tt[200];
        fp_t zero;
        fp_set_zero(&zero);

        tt[0] = x[i];
        uint32_t z0 = fp_equals(&tt[0], &zero);
        fp_select(&tt[0], &tt[0], &FP_ONE, z0);

        for (size_t j = 1; j < blen; j++) {
            tt[j] = x[i + j];
            uint32_t z = fp_equals(&tt[j], &zero);
            fp_select(&tt[j], &tt[j], &FP_ONE, z);
            fp_mul(&tt[j], &tt[j], &tt[j - 1]);
        }

        fp_t k;
        fp_inv(&k, &tt[blen - 1]);

        // Backward pass
        for (size_t j = blen; j-- > 1;) {
            fp_t cur = x[i + j];
            uint32_t z = fp_equals(&cur, &zero);
            fp_select(&cur, &cur, &FP_ONE, z);

            fp_t prod;
            fp_mul(&prod, &k, &tt[j - 1]);
            fp_select(&x[i + j], &x[i + j], &prod, ~z);
            fp_mul(&k, &k, &cur);
        }
        fp_select(&x[i], &x[i], &k, ~z0);
        i += blen;
    }
}
"""

    def generate_decode(self) -> str:
        return """\
static void
fp_decode_nocheck(fp_t *out, const uint8_t in[FP_ENCODED_BYTES])
{
    fp_t raw = { { 0 } };

    for (size_t i = 0; i < FP_LIMBS - 1; i++) {
        uint64_t w = 0;
        for (size_t j = 0; j < 8; j++)
            w |= (uint64_t)in[8 * i + j] << (8 * j);
        raw.limb[i] = w;
    }

    {
        const size_t last = FP_LIMBS - 1;
        const size_t last_bytes = FP_ENCODED_BYTES - 8 * (FP_LIMBS - 1);
        uint64_t w = 0;
        for (size_t j = 0; j < last_bytes; j++)
            w |= (uint64_t)in[8 * last + j] << (8 * j);
        raw.limb[last] = w;
    }

    *out = raw;
}

uint32_t
fp_decode(fp_t *out, const uint8_t in[FP_ENCODED_BYTES])
{
    fp_t raw;
    fp_decode_nocheck(&raw, in);

    uint64_t borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint64_t d;
        borrow = subborrow_u64((unsigned char)borrow,
                                       raw.limb[i], FP_MODULUS[i], &d);
    }

    uint64_t mask = (uint64_t)0 - borrow;
    for (size_t i = 0; i < FP_LIMBS; i++)
        raw.limb[i] &= mask;

    fp_mul(out, &raw, &FP_R2_ELEM);
    return (uint32_t)mask;
}
"""

    def generate_decode_reduce(self) -> str:
        return """\
void
fp_decode_reduce(fp_t *out, const uint8_t *in, size_t len)
{
    fp_t t = { { 0 } };
    uint8_t tmp[FP_ENCODED_BYTES];

    if (len == 0) {
        *out = t;
        return;
    }

    /* Match Rust set_decode_reduce(): process fixed-size chunks MSB first. */
    size_t rem = len % FP_DECODE_REDUCE_CHUNK;
    if (rem == 0)
        rem = FP_DECODE_REDUCE_CHUNK;

    size_t pos = len - rem;
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, in + pos, rem);
    fp_decode_nocheck(&t, tmp);

    while (pos != 0) {
        pos -= FP_DECODE_REDUCE_CHUNK;
        memset(tmp, 0, sizeof(tmp));
        memcpy(tmp, in + pos, FP_DECODE_REDUCE_CHUNK);

        fp_t d;
        fp_decode_nocheck(&d, tmp);
        fp_mul(&t, &t, &FP_TDEC_ELEM);
        fp_add(&t, &t, &d);
    }

    fp_mul(out, &t, &FP_R2_ELEM);
}
"""

    def generate_encode(self) -> str:
        return """\
void
fp_encode(uint8_t out[FP_ENCODED_BYTES], const fp_t *x)
{
    fp_t t = *x;
    fp_internal_reduce(&t);

    for (size_t i = 0; i < FP_LIMBS - 1; i++) {
        uint64_t w = t.limb[i];
        for (size_t j = 0; j < 8; j++) {
            out[8 * i + j] = (uint8_t)(w >> (8 * j));
        }
    }

    {
        const size_t last = FP_LIMBS - 1;
        const size_t last_bytes = FP_ENCODED_BYTES - 8 * (FP_LIMBS - 1);
        uint64_t w = t.limb[last];

        for (size_t j = 0; j < last_bytes; j++) {
            out[8 * last + j] = (uint8_t)(w >> (8 * j));
        }
    }
}
"""

    def generate_less_than(self) -> str:
        return """\
inline uint32_t
fp_less_than(const fp_t *x1, const fp_t *x2)
{
    uint8_t buf1[FP_ENCODED_BYTES];
    uint8_t buf2[FP_ENCODED_BYTES];

    fp_encode(buf1, x1);
    fp_encode(buf2, x2);

    uint32_t result = 0;
    uint32_t all_equal_so_far = UINT32_MAX;

    for (size_t idx = FP_ENCODED_BYTES; idx-- > 0;) {
        uint32_t less = ct_lt_u8(buf1[idx], buf2[idx]);
        uint32_t equal = ct_eq_u8(buf1[idx], buf2[idx]);
        result |= all_equal_so_far & less;
        all_equal_so_far &= equal;
    }

    return result;
}
"""


    def generate_source(self) -> str:
        methods = [
            self.generate_constants(),
            self.generate_set_zero(),
            self.generate_set_one(),
            self.generate_copy(),
            self.generate_set_small(),
            self.generate_add(),
            self.generate_sub(),
            self.generate_neg(),
            self.generate_double(),
            self.generate_half(),
            self.generate_equals(),
            self.generate_is_zero(),
            self.generate_ct_helpers(),
            self.generate_less_than(),
            self.generate_reduce(),
            self.generate_mul_small(),
            self.generate_mul(),
            self.generate_sqr(),
            self.generate_n_sqr(),
            self.generate_sum_of_products(),
            self.generate_difference_of_products(),
            self.generate_binary_gcd_helpers(),
            self.generate_inv(),
            self.generate_legendre(),
            self.generate_is_square(),
            self.generate_batch_inversion(),
            self.generate_vartime_pow(),
            self.generate_sqrt(),
            self.generate_exp3div4(),
            self.generate_decode(),
            self.generate_decode_reduce(),
            self.generate_encode(),
        ]
        return '#include "fp.h"\n#include "util_64.h"\n\n#include <stddef.h>\n#include <stdint.h>\n#include <string.h>\n\n' + '\n'.join(methods)

    def generate(self, include_dir: Path, source_dir: Path):
        include_dir.mkdir(parents=True, exist_ok=True)
        source_dir.mkdir(parents=True, exist_ok=True)
        (include_dir / "fp_defs.h").write_text(self.generate_defs_header())
        (source_dir / "fp.c").write_text(self.generate_source())


def parse_int(s: str) -> int:
    return eval(s)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("prime", help="prime as decimal or 0x-prefixed integer")
    ap.add_argument("--include-dir", required=True)
    ap.add_argument("--source-dir", required=True)
    args = ap.parse_args()

    FieldGenerator(parse_int(args.prime)).generate(
        Path(args.include_dir), Path(args.source_dir)
    )

if __name__ == "__main__":
    main()
