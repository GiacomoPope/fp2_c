#!/usr/bin/env python3
import argparse
from pathlib import Path

MASK32 = (1 << 32) - 1


class FieldGeneratorU32:
    def __init__(self, p: int):
        if p <= 3:
            raise ValueError("prime must be > 3")
        if p % 4 != 3:
            raise ValueError("this generator currently requires p == 3 (mod 4)")

        self.p = p
        self.bits = p.bit_length()
        self.n = (self.bits + 31) // 32

        if 2 * p - 4 >= (1 << (32 * self.n)):
            raise ValueError("sum/difference of products requires 2*p - 4 to fit in N limbs")

        self.encoded_length = (self.bits + 7) // 8
        self.modulus = [(p >> (32 * i)) & MASK32 for i in range(self.n)]

        self.r = pow(2, 32 * self.n, p)
        self.r2 = pow(2, 64 * self.n, p)

        m64 = (self.bits + 63) // 64
        self.clen = self.encoded_length - 1 if m64 == 1 else 8 * (m64 - 1)
        self.tdec = pow(2, 32 * self.n + 8 * self.clen, p)

        self.sqrt_exp = (p + 1) // 4
        self.p_minus_three_div_four = (p - 3) // 4

        self.p0i = (-pow(self.modulus[0], -1, 1 << 32)) & MASK32
        self.num1 = (2 * self.bits - 18) // 15
        self.num2 = 2 * self.bits - 15 * self.num1 - 2
        tfix_exp = 64 * self.n + 17 * self.num1 + 32 - self.num2
        self.tfixdiv = pow(2, tfix_exp, p)

    @staticmethod
    def limbs(x: int, n: int):
        return [(x >> (32 * i)) & MASK32 for i in range(n)]

    @staticmethod
    def c_u32(x: int) -> str:
        return f"UINT32_C(0x{x & MASK32:08x})"

    def array_literal(self, xs):
        return "{ " + ", ".join(self.c_u32(x) for x in xs) + " }"

    def p0i_expr(self, var: str) -> str:
        if self.p0i == 1:
            return var
        return f"{var} * FP_P0I"

    def mul_add_by_constant(self, lo, hi, x, const, z1, z2=None):
        if z2 is None:
            return f"mul_add(&{lo}, &{hi}, {x}, {self.c_u32(const)}, {z1});"
        return f"mul_add2(&{lo}, &{hi}, {x}, {self.c_u32(const)}, {z1}, {z2});"

    def generate_defs_header(self) -> str:
        return f"""\
#ifndef FP_DEFS_H
#define FP_DEFS_H

#include <stdint.h>

#define FP_LIMBS {self.n}
#define FP_BITS {self.bits}
#define FP_ENCODED_BYTES {self.encoded_length}
#define FP_DECODE_REDUCE_CHUNK {self.clen}

/* The one thing that actually differs between the 64- and 32-bit-limb
   backends' shared include/fp.h: the limb (and fp_pow_pubexp exponent
   word) type. */
typedef uint32_t fp_limb_t;

#endif /* FP_DEFS_H */
"""

    def generate_constants(self) -> str:
        p0i_line = (
            "" if self.p0i == 1 else f"static const uint32_t FP_P0I = {self.c_u32(self.p0i)};\n"
        )
        return f"""\
static const uint32_t FP_MODULUS[FP_LIMBS] = {self.array_literal(self.modulus)};
{p0i_line}static const fp_t FP_ONE = {{ .limb = {self.array_literal(self.limbs(self.r, self.n))} }};
static const fp_t FP_R2_ELEM = {{ .limb = {self.array_literal(self.limbs(self.r2, self.n))} }};
static const fp_t FP_TDEC_ELEM = {{ .limb = {self.array_literal(self.limbs(self.tdec, self.n))} }};
static const uint32_t FP_SQRT_EXP[FP_LIMBS] = {self.array_literal(self.limbs(self.sqrt_exp, self.n))};
static const uint32_t FP_P_MINUS_3_DIV_FOUR[FP_LIMBS] = {self.array_literal(self.limbs(self.p_minus_three_div_four, self.n))};
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
        c1 = addcarry_u32(c1, a->limb[i], b->limb[i],
                                  &t.limb[i]);
    }

    for (size_t i = 0; i < FP_LIMBS; i++) {
        c2 = subborrow_u32(c2, t.limb[i],
                                   FP_MODULUS[i], &t.limb[i]);
    }

    uint32_t mask = (uint32_t)c1 - (uint32_t)c2;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        carry = addcarry_u32(carry, t.limb[i],
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
        borrow = subborrow_u32(borrow, a->limb[i],
                                      b->limb[i], &t.limb[i]);
    }

    uint32_t mask = (uint32_t)0 - borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        carry = addcarry_u32(carry, t.limb[i],
                                     FP_MODULUS[i] & mask, &t.limb[i]);
    }

    *r = t;
}
"""

    def generate_hadamard(self) -> str:
        return """\
inline void
fp_hadamard(fp_t *r1, fp_t *r2, const fp_t *a, const fp_t *b)
{
    fp_t sum, diff;
    unsigned char c1 = 0;
    unsigned char c2 = 0;

    /* a and b are each read once, feeding two independent carry chains. */
    for (size_t i = 0; i < FP_LIMBS; i++) {
        c1 = addcarry_u32(c1, a->limb[i], b->limb[i], &sum.limb[i]);
        c2 = subborrow_u32(c2, a->limb[i], b->limb[i], &diff.limb[i]);
    }

    unsigned char cs = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        cs = subborrow_u32(cs, sum.limb[i], FP_MODULUS[i], &sum.limb[i]);
    }
    uint32_t mask_s = (uint32_t)c1 - (uint32_t)cs;
    unsigned char cas = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        cas = addcarry_u32(cas, sum.limb[i], FP_MODULUS[i] & mask_s, &sum.limb[i]);
    }
    (void)cas;

    uint32_t mask_d = (uint32_t)0 - (uint32_t)c2;
    unsigned char cad = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        cad = addcarry_u32(cad, diff.limb[i], FP_MODULUS[i] & mask_d, &diff.limb[i]);
    }
    (void)cad;

    *r1 = sum;
    *r2 = diff;
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
        borrow = subborrow_u32(borrow, 0, a->limb[i], &t.limb[i]);
    }

    uint32_t mask = (uint32_t)0 - borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        carry = addcarry_u32(carry, t.limb[i],
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
        uint32_t w = t.limb[i];
        uint32_t d = (w << 1) | tb;
        tb = (unsigned char)(w >> 31);
        cc = subborrow_u32(cc, d, FP_MODULUS[i], &t.limb[i]);
    }

    uint32_t mm = (uint32_t)tb - (uint32_t)cc;
    cc = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        cc = addcarry_u32(cc, t.limb[i],
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
    uint32_t m = (uint32_t)0 - (t.limb[0] & 1u);
    unsigned char carry = 0;

    for (size_t i = 0; i < FP_LIMBS; i++)
        carry = addcarry_u32(carry, t.limb[i],
                                     FP_MODULUS[i] & m, &t.limb[i]);

    for (size_t i = 0; i + 1 < FP_LIMBS; i++)
        t.limb[i] = (t.limb[i] >> 1) | (t.limb[i + 1] << 31);
    t.limb[FP_LIMBS - 1] = (t.limb[FP_LIMBS - 1] >> 1) |
                           ((uint32_t)carry << 31);
    *out = t;
}
"""

    def generate_equals(self) -> str:
        return """\
inline uint32_t
fp_equals(const fp_t *a, const fp_t *b)
{
    uint32_t r = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        r |= a->limb[i] ^ b->limb[i];

    return (uint32_t)(((r | (uint32_t)(0 - r)) >> 31) - 1);
}
"""

    def generate_is_zero(self) -> str:
        return """\
inline uint32_t
fp_is_zero(const fp_t *a)
{
    uint32_t x = a->limb[0];

    for (size_t i = 1; i < FP_LIMBS; i++) {
        x |= a->limb[i];
    }

    x |= (uint32_t)(0 - x);

    uint32_t s = (uint32_t)(((int32_t)x) >> 31);
    return (uint32_t)(~s);
}
"""

    def generate_ct_helpers(self) -> str:
        return """\
inline void
fp_select(fp_t *out, const fp_t *a, const fp_t *b, uint32_t ctl)
{
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint32_t wa = a->limb[i];
        uint32_t wb = b->limb[i];
        out->limb[i] = wa ^ (ctl & (wa ^ wb));
    }
}

inline void
fp_cond_swap(fp_t *a, fp_t *b, uint32_t ctl)
{
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint32_t wa = a->limb[i];
        uint32_t wb = b->limb[i];
        uint32_t wc = ctl & (wa ^ wb);
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
        uint32_t f = __P0I_EXPR__;
        uint32_t lo, cc;

        mul_add(&lo, &cc,
                        f, FP_MODULUS[0], x->limb[0]);

        for (size_t j = 1; j < FP_LIMBS; j++) {
            uint32_t hi;
            mul_add2(&lo, &hi,
                             f, FP_MODULUS[j],
                             x->limb[j], cc);
            x->limb[j - 1] = lo;
            cc = hi;
        }

        x->limb[FP_LIMBS - 1] = cc;
    }
}
""".replace("__P0I_EXPR__", self.p0i_expr("x->limb[0]"))

    def generate_mul_small_n(self) -> str:
        N = self.n
        mod = self.modulus

        lines = [
            "inline void",
            "fp_mul(fp_t *out, const fp_t *a, const fp_t *b)",
            "{",
            "    fp_t t = { { 0 } };",
            "    unsigned char cch = 0;",
            "",
        ]

        for i in range(N):
            lines.append(f"    /* i = {i}: t += b[{i}] * a, folding in one Montgomery reduction limb */")
            lines.append("    {")
            lines.append("        uint32_t lo, hi, cc1, cc2;")
            lines.append("")
            lines.append(f"        mul_add(&lo, &cc1, b->limb[{i}], a->limb[0], t.limb[0]);")
            lines.append("        t.limb[0] = lo;")
            for j in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, b->limb[{i}], a->limb[{j}], t.limb[{j}], cc1);"
                )
                lines.append(f"        t.limb[{j}] = lo;")
                lines.append("        cc1 = hi;")

            lines.append("")
            lines.append(f"        uint32_t q = {self.p0i_expr('t.limb[0]')};")
            lines.append("        " + self.mul_add_by_constant("lo", "cc2", "q", mod[0], "t.limb[0]"))
            for j in range(1, N):
                lines.append(
                    "        " + self.mul_add_by_constant("lo", "hi", "q", mod[j], f"t.limb[{j}]", "cc2")
                )
                lines.append(f"        t.limb[{j - 1}] = lo;")
                lines.append("        cc2 = hi;")

            lines.append("")
            lines.append("        uint32_t top;")
            lines.append("        unsigned char carry = addcarry_u32(0, cc1, cc2, &top);")
            lines.append("        carry = addcarry_u32(carry, top, cch, &top);")
            lines.append(f"        t.limb[{N - 1}] = top;")
            lines.append("        cch = carry;")
            lines.append("    }")
            lines.append("")

        lines.append("    unsigned char borrow = 0;")
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u32(borrow, t.limb[{i}], {self.c_u32(mod[i])}, &t.limb[{i}]);"
            )

        lines.append("")
        lines.append("    uint32_t mask = (uint32_t)cch - (uint32_t)borrow;")
        lines.append("    unsigned char carry = 0;")
        for i in range(N):
            lines.append(
                f"    carry = addcarry_u32(carry, t.limb[{i}], {self.c_u32(mod[i])} & mask, &t.limb[{i}]);"
            )

        lines.append("")
        lines.append("    *out = t;")
        lines.append("}")

        return "\n".join(lines) + "\n"

    def generate_mul_large_n(self) -> str:
        return """\
inline void
fp_mul(fp_t *out, const fp_t *a, const fp_t *b)
{
    fp_t t = { { 0 } };
    unsigned char cch = 0;

    #pragma clang loop unroll(full)
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint32_t f = b->limb[i];
        uint32_t lo, cc1;

        mul_add(&lo, &cc1,
                        f, a->limb[0], t.limb[0]);

        uint32_t g = __P0I_EXPR__;
        uint32_t cc2;

        mul_add(&lo, &cc2,
                        g, FP_MODULUS[0], lo);

        #pragma clang loop unroll(full)
        for (size_t j = 1; j < FP_LIMBS; j++) {
            uint32_t d, hi1, hi2;

            mul_add2(&d, &hi1,
                             f, a->limb[j],
                             t.limb[j], cc1);
            cc1 = hi1;

            mul_add2(&d, &hi2,
                             g, FP_MODULUS[j],
                             d, cc2);
            cc2 = hi2;

            t.limb[j - 1] = d;
        }

        uint32_t top;
        unsigned char carry = addcarry_u32(0, cc1, cc2, &top);
        carry = addcarry_u32(carry, top, cch, &top);
        t.limb[FP_LIMBS - 1] = top;
        cch = carry;
    }

    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        borrow = subborrow_u32(borrow, t.limb[i],
                                       FP_MODULUS[i], &t.limb[i]);
    }

    uint32_t mask = (uint32_t)cch - (uint32_t)borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        carry = addcarry_u32(carry, t.limb[i],
                                     FP_MODULUS[i] & mask, &t.limb[i]);
    }

    *out = t;
}
""".replace("__P0I_EXPR__", self.p0i_expr("lo"))

    @property
    def has_single_limb_p1(self) -> bool:
        return self.n >= 2 and all(m == MASK32 for m in self.modulus[:-1])

    def generate_mul_p1_rotate(self) -> str:
        N = self.n
        mod = self.modulus
        p1_top = mod[-1] + 1

        var_names = [f"t{k}" for k in range(N)]
        lines = [
            "void",
            "fp_mul(fp_t *out, const fp_t *a, const fp_t *b)",
            "{",
            "    uint32_t " + ", ".join(f"{v} = 0" for v in var_names) + ";",
            "    uint32_t cch = 0;",
            "",
        ]

        # phys[j]: the C variable currently standing in for logical limb j.
        phys = list(var_names)
        for i in range(N):
            lines.append(f"    /* i = {i}: fold in b[{i}]*a, then one p+1-based reduction limb */")
            lines.append("    {")
            lines.append("        uint32_t lo, hi, cc1;")
            lines.append("")
            lines.append(f"        mul_add(&lo, &cc1, b->limb[{i}], a->limb[0], {phys[0]});")
            lines.append(f"        {phys[0]} = lo;")
            for j in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, b->limb[{i}], a->limb[{j}], {phys[j]}, cc1);"
                )
                lines.append(f"        {phys[j]} = lo;")
                lines.append("        cc1 = hi;")

            lines.append("")
            q_var, top_var = phys[0], phys[-1]
            lines.append(f"        uint32_t q = {self.p0i_expr(q_var)};")
            lines.append("        uint32_t prod_lo, prod_hi;")
            lines.append(f"        mul_u32(&prod_lo, &prod_hi, q, {self.c_u32(p1_top)});")
            lines.append(
                f"        unsigned char carry_mid = addcarry_u32(0, {top_var}, prod_lo, &{top_var});"
            )
            lines.append("        uint32_t cc1_new;")
            lines.append("        unsigned char cA = addcarry_u32(0, cc1, prod_hi, &cc1_new);")
            lines.append("        unsigned char cB = addcarry_u32(0, cc1_new, carry_mid, &cc1_new);")
            lines.append("        unsigned char cc2 = (unsigned char)(cA + cB);")
            lines.append("        uint32_t top;")
            lines.append("        unsigned char cC = addcarry_u32(0, cc1_new, cch, &top);")
            lines.append("        cch = (uint32_t)cC + cc2;")
            lines.append(f"        {q_var} = top;")
            lines.append("    }")
            lines.append("")

            phys = phys[1:] + [phys[0]]

        assert phys == var_names, "rotation must cycle back after FP_LIMBS steps"

        lines.append("    unsigned char borrow = 0;")
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u32(borrow, {var_names[i]}, {self.c_u32(mod[i])}, &{var_names[i]});"
            )
        lines.append("    uint32_t mask = cch - (uint32_t)borrow;")
        lines.append("    unsigned char carry = 0;")
        for i in range(N):
            lines.append(
                f"    carry = addcarry_u32(carry, {var_names[i]}, {self.c_u32(mod[i])} & mask, &{var_names[i]});"
            )
        lines.append("    (void)carry;")
        lines.append("")
        lines.append("    fp_t r = { { " + ", ".join(var_names) + " } };")
        lines.append("    *out = r;")
        lines.append("}")

        return "\n".join(lines) + "\n"

    def generate_mul(self) -> str:
        if self.has_single_limb_p1:
            return self.generate_mul_p1_rotate()
        if self.n >= 15:
            return self.generate_mul_large_n()
        return self.generate_mul_small_n()

    _SQR_PRODUCT_BODY = """\
    uint32_t t[FP_LIMBS * 2] = { 0 };

    uint32_t f = a->limb[0];
    uint32_t cc;
    mul_u32(&t[1], &cc, f, a->limb[1]);
    #pragma clang loop unroll(full)
    for (size_t j = 2; j < FP_LIMBS; j++) {
        uint32_t hi;
        mul_add(&t[j], &hi, f, a->limb[j], cc);
        cc = hi;
    }
    t[FP_LIMBS] = cc;

    #pragma clang loop unroll(full)
    for (size_t i = 1; i < FP_LIMBS - 1; i++) {
        f = a->limb[i];
        uint32_t hi;
        mul_add(&t[2 * i + 1], &cc,
                        f, a->limb[i + 1], t[2 * i + 1]);
        for (size_t j = i + 2; j < FP_LIMBS; j++) {
            mul_add2(&t[i + j], &hi,
                             f, a->limb[j], t[i + j], cc);
            cc = hi;
        }
        t[i + FP_LIMBS] = cc;
    }

    cc = 0;
    #pragma clang loop unroll(full)
    for (size_t i = 1; i < FP_LIMBS * 2 - 1; i++) {
        uint32_t w = t[i];
        uint32_t hi = w >> 31;
        t[i] = (w << 1) | cc;
        cc = hi;
    }
    t[FP_LIMBS * 2 - 1] = cc;

    cc = 0;
    #pragma clang loop unroll(full)
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint32_t lo, hi;
        mul_u32(&lo, &hi, a->limb[i], a->limb[i]);
        uint32_t ee = addcarry_u32((unsigned char)cc, lo, t[2 * i], &t[2 * i]);
        ee = addcarry_u32(ee, hi, t[2 * i + 1], &t[2 * i + 1]);
        cc = ee;
    }
"""

    def generate_sqr_generic(self) -> str:
        return (
            """\
inline void
fp_sqr(fp_t *out, const fp_t *a)
{
"""
            + self._SQR_PRODUCT_BODY
            + """
    fp_t lo = { { 0 } };
    fp_t hi = { { 0 } };
    for (size_t i = 0; i < FP_LIMBS; i++) {
        lo.limb[i] = t[i];
        hi.limb[i] = t[i + FP_LIMBS];
    }
    fp_internal_reduce(&lo);
    fp_add(out, &lo, &hi);
}
"""
        )

    def generate_sqr_p1_fused(self) -> str:
        N = self.n
        mod = self.modulus
        p1_top = mod[-1] + 1

        lines = [
            "void",
            "fp_sqr(fp_t *out, const fp_t *a)",
            "{",
        ]
        lines.append(self._SQR_PRODUCT_BODY.rstrip("\n"))
        lines.append("")
        lines.append("    uint32_t cch = 0;")
        for row in range(N):
            pos = row + N - 1
            lines.append(f"    /* round {row}: fold t[{row}] into the high half via t[{row}]*p1_top */")
            lines.append("    {")
            lines.append(f"        uint32_t q = {self.p0i_expr(f't[{row}]')};")
            lines.append("        uint32_t lo, hi;")
            lines.append(f"        mul_u32(&lo, &hi, q, {self.c_u32(p1_top)});")
            lines.append(f"        unsigned char c = addcarry_u32(0, t[{pos}], lo, &t[{pos}]);")
            lines.append(f"        c = addcarry_u32(c, t[{pos + 1}], hi, &t[{pos + 1}]);")
            for k in range(pos + 2, 2 * N):
                lines.append(f"        c = addcarry_u32(c, t[{k}], 0, &t[{k}]);")
            lines.append("        cch += (uint32_t)c;")
            lines.append("    }")
        lines.append("")
        lines.append("    unsigned char borrow = 0;")
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u32(borrow, t[FP_LIMBS + {i}], {self.c_u32(mod[i])}, &t[FP_LIMBS + {i}]);"
            )
        lines.append("    uint32_t mask = cch - (uint32_t)borrow;")
        lines.append("    unsigned char carry = 0;")
        for i in range(N):
            lines.append(
                f"    carry = addcarry_u32(carry, t[FP_LIMBS + {i}], {self.c_u32(mod[i])} & mask, &t[FP_LIMBS + {i}]);"
            )
        lines.append("    (void)carry;")
        lines.append("")
        lines.append(
            "    fp_t r = { { " + ", ".join(f"t[FP_LIMBS + {i}]" for i in range(N)) + " } };"
        )
        lines.append("    *out = r;")
        lines.append("}")

        return "\n".join(lines) + "\n"

    def generate_sqr(self) -> str:
        if self.has_single_limb_p1:
            return self.generate_sqr_p1_fused()
        return self.generate_sqr_generic()

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
        return r"""
inline void
fp_mul_small(fp_t *out, const fp_t *a, int32_t k)
{
    /* Get the absolute value of k, while retaining its sign. */
    uint32_t sk = (uint32_t)((int64_t)k >> 31);
    uint32_t ak = (((uint32_t)k ^ sk) - sk);

    uint8_t enc[4] = {
        (uint8_t)ak, (uint8_t)(ak >> 8), (uint8_t)(ak >> 16), (uint8_t)(ak >> 24)
    };
    fp_t scalar;
    fp_decode_reduce(&scalar, enc, sizeof(enc));

    fp_mul(out, a, &scalar);
    fp_cond_neg(out, sk);
}
"""

    def generate_sum_of_products_large_n(self) -> str:
        return """\
inline void
fp_sum_of_products(fp_t *out, const fp_t *a1, const fp_t *b1,
                   const fp_t *a2, const fp_t *b2)
{
    fp_t u = { { 0 } };
    unsigned char cch = 0;

    for (size_t j = 0; j < FP_LIMBS; j++) {
        uint32_t cc1, cc2, cc3, lo;
        mul_add(&lo, &cc1, a1->limb[j], b1->limb[0], u.limb[0]);
        u.limb[0] = lo;
        for (size_t k = 1; k < FP_LIMBS; k++) {
            uint32_t hi;
            mul_add2(&lo, &hi, a1->limb[j], b1->limb[k], u.limb[k], cc1);
            u.limb[k] = lo;
            cc1 = hi;
        }
        mul_add(&lo, &cc2, a2->limb[j], b2->limb[0], u.limb[0]);
        u.limb[0] = lo;
        for (size_t k = 1; k < FP_LIMBS; k++) {
            uint32_t hi;
            mul_add2(&lo, &hi, a2->limb[j], b2->limb[k], u.limb[k], cc2);
            u.limb[k] = lo;
            cc2 = hi;
        }

        uint32_t q = __P0I_EXPR__;
        mul_add(&lo, &cc3, q, FP_MODULUS[0], u.limb[0]);
        for (size_t k = 1; k < FP_LIMBS; k++) {
            uint32_t hi;
            mul_add2(&lo, &hi, q, FP_MODULUS[k], u.limb[k], cc3);
            u.limb[k - 1] = lo;
            cc3 = hi;
        }

        uint32_t top;
        unsigned char c1 = addcarry_u32(cch, cc1, cc2, &top);
        unsigned char c2 = addcarry_u32(0, top, cc3, &top);
        cch = (unsigned char)(c1 + c2);
        u.limb[FP_LIMBS - 1] = top;
    }

    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        borrow = subborrow_u32(borrow, u.limb[i], FP_MODULUS[i], &u.limb[i]);
    uint32_t mask = (uint32_t)cch - (uint32_t)borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        carry = addcarry_u32(carry, u.limb[i], FP_MODULUS[i] & mask, &u.limb[i]);
    (void)carry;
    *out = u;
}
""".replace("__P0I_EXPR__", self.p0i_expr("u.limb[0]"))

    def generate_sum_of_products_small_n(self) -> str:
        N = self.n
        mod = self.modulus

        lines = [
            "inline void",
            "fp_sum_of_products(fp_t *out, const fp_t *a1, const fp_t *b1,",
            "                   const fp_t *a2, const fp_t *b2)",
            "{",
            "    fp_t u = { { 0 } };",
            "    unsigned char cch = 0;",
            "",
        ]

        for j in range(N):
            lines.append(f"    /* j = {j}: u += a1[{j}]*b1 + a2[{j}]*b2, folding in one reduction limb */")
            lines.append("    {")
            lines.append("        uint32_t lo, hi, cc1, cc2, cc3;")
            lines.append("")
            lines.append(f"        mul_add(&lo, &cc1, a1->limb[{j}], b1->limb[0], u.limb[0]);")
            lines.append("        u.limb[0] = lo;")
            for k in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, a1->limb[{j}], b1->limb[{k}], u.limb[{k}], cc1);"
                )
                lines.append(f"        u.limb[{k}] = lo;")
                lines.append("        cc1 = hi;")

            lines.append("")
            lines.append(f"        mul_add(&lo, &cc2, a2->limb[{j}], b2->limb[0], u.limb[0]);")
            lines.append("        u.limb[0] = lo;")
            for k in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, a2->limb[{j}], b2->limb[{k}], u.limb[{k}], cc2);"
                )
                lines.append(f"        u.limb[{k}] = lo;")
                lines.append("        cc2 = hi;")

            lines.append("")
            lines.append(f"        uint32_t q = {self.p0i_expr('u.limb[0]')};")
            lines.append("        " + self.mul_add_by_constant("lo", "cc3", "q", mod[0], "u.limb[0]"))
            for k in range(1, N):
                lines.append(
                    "        " + self.mul_add_by_constant("lo", "hi", "q", mod[k], f"u.limb[{k}]", "cc3")
                )
                lines.append(f"        u.limb[{k - 1}] = lo;")
                lines.append("        cc3 = hi;")

            lines.append("")
            lines.append("        uint32_t top;")
            lines.append("        unsigned char c1 = addcarry_u32(cch, cc1, cc2, &top);")
            lines.append("        unsigned char c2 = addcarry_u32(0, top, cc3, &top);")
            lines.append("        cch = (unsigned char)(c1 + c2);")
            lines.append(f"        u.limb[{N - 1}] = top;")
            lines.append("    }")

        lines.append("")
        lines.append("    unsigned char borrow = 0;")
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u32(borrow, u.limb[{i}], {self.c_u32(mod[i])}, &u.limb[{i}]);"
            )
        lines.append("    uint32_t mask = (uint32_t)cch - (uint32_t)borrow;")
        lines.append("    unsigned char carry = 0;")
        for i in range(N):
            lines.append(
                f"    carry = addcarry_u32(carry, u.limb[{i}], {self.c_u32(mod[i])} & mask, &u.limb[{i}]);"
            )
        lines.append("    (void)carry;")
        lines.append("    *out = u;")
        lines.append("}")

        return "\n".join(lines) + "\n"

    def generate_sum_of_products_p1_rotate(self) -> str:
        N = self.n
        mod = self.modulus
        p1_top = mod[-1] + 1

        var_names = [f"u{k}" for k in range(N)]
        lines = [
            "inline void",
            "fp_sum_of_products(fp_t *out, const fp_t *a1, const fp_t *b1,",
            "                   const fp_t *a2, const fp_t *b2)",
            "{",
            "    uint32_t " + ", ".join(f"{v} = 0" for v in var_names) + ";",
            "    uint32_t cch = 0;",
            "",
        ]

        phys = list(var_names)
        for j in range(N):
            lines.append(f"    /* j = {j}: u += a1[{j}]*b1 + a2[{j}]*b2, then one p+1-based reduction limb */")
            lines.append("    {")
            lines.append("        uint32_t lo, hi, cc1, cc2;")
            lines.append("")
            lines.append(f"        mul_add(&lo, &cc1, a1->limb[{j}], b1->limb[0], {phys[0]});")
            lines.append(f"        {phys[0]} = lo;")
            for k in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, a1->limb[{j}], b1->limb[{k}], {phys[k]}, cc1);"
                )
                lines.append(f"        {phys[k]} = lo;")
                lines.append("        cc1 = hi;")

            lines.append("")
            lines.append(f"        mul_add(&lo, &cc2, a2->limb[{j}], b2->limb[0], {phys[0]});")
            lines.append(f"        {phys[0]} = lo;")
            for k in range(1, N):
                lines.append(
                    f"        mul_add2(&lo, &hi, a2->limb[{j}], b2->limb[{k}], {phys[k]}, cc2);"
                )
                lines.append(f"        {phys[k]} = lo;")
                lines.append("        cc2 = hi;")

            lines.append("")
            q_var, top_var = phys[0], phys[-1]
            lines.append(f"        uint32_t q = {self.p0i_expr(q_var)};")
            lines.append("        uint32_t prod_lo, prod_hi;")
            lines.append(f"        mul_u32(&prod_lo, &prod_hi, q, {self.c_u32(p1_top)});")
            lines.append(
                f"        unsigned char carry_mid = addcarry_u32(0, {top_var}, prod_lo, &{top_var});"
            )
            lines.append("        uint32_t cc_combined;")
            lines.append("        unsigned char cA = addcarry_u32(0, cc1, cc2, &cc_combined);")
            lines.append("        unsigned char cB = addcarry_u32(0, cc_combined, prod_hi, &cc_combined);")
            lines.append("        unsigned char cD = addcarry_u32(0, cc_combined, carry_mid, &cc_combined);")
            lines.append("        unsigned char cc2b = (unsigned char)(cA + cB + cD);")
            lines.append("        uint32_t top;")
            lines.append("        unsigned char cC = addcarry_u32(0, cc_combined, cch, &top);")
            lines.append("        cch = (uint32_t)cC + cc2b;")
            lines.append(f"        {q_var} = top;")
            lines.append("    }")
            lines.append("")

            phys = phys[1:] + [phys[0]]

        assert phys == var_names, "rotation must cycle back after FP_LIMBS steps"

        lines.append("    unsigned char borrow = 0;")
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u32(borrow, {var_names[i]}, {self.c_u32(mod[i])}, &{var_names[i]});"
            )
        lines.append("    uint32_t mask = cch - (uint32_t)borrow;")
        lines.append("    unsigned char carry = 0;")
        for i in range(N):
            lines.append(
                f"    carry = addcarry_u32(carry, {var_names[i]}, {self.c_u32(mod[i])} & mask, &{var_names[i]});"
            )
        lines.append("    (void)carry;")
        lines.append("")
        lines.append("    fp_t r = { { " + ", ".join(var_names) + " } };")
        lines.append("    *out = r;")
        lines.append("}")

        return "\n".join(lines) + "\n"

    def generate_sum_of_products(self) -> str:
        if self.has_single_limb_p1:
            return self.generate_sum_of_products_p1_rotate()
        if self.n >= 15:
            return self.generate_sum_of_products_large_n()
        return self.generate_sum_of_products_small_n()

    def generate_difference_of_products_large_n(self) -> str:
        return """\
inline void
fp_difference_of_products(fp_t *out, const fp_t *a1, const fp_t *b1,
                          const fp_t *a2, const fp_t *b2)
{
    fp_t nb2 = { { 0 } };
    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        borrow = subborrow_u32(borrow, FP_MODULUS[i], b2->limb[i], &nb2.limb[i]);
    (void)borrow;
    fp_sum_of_products(out, a1, b1, a2, &nb2);
}
"""

    def generate_difference_of_products_small_n(self) -> str:
        N = self.n
        mod = self.modulus

        lines = [
            "inline void",
            "fp_difference_of_products(fp_t *out, const fp_t *a1, const fp_t *b1,",
            "                          const fp_t *a2, const fp_t *b2)",
            "{",
            "    fp_t nb2 = { { 0 } };",
            "    unsigned char borrow = 0;",
        ]
        for i in range(N):
            lines.append(
                f"    borrow = subborrow_u32(borrow, {self.c_u32(mod[i])}, b2->limb[{i}], &nb2.limb[{i}]);"
            )
        lines.append("    (void)borrow;")
        lines.append("    fp_sum_of_products(out, a1, b1, a2, &nb2);")
        lines.append("}")

        return "\n".join(lines) + "\n"

    def generate_difference_of_products(self) -> str:
        if self.n >= 15:
            return self.generate_difference_of_products_large_n()
        return self.generate_difference_of_products_small_n()

    def generate_vartime_pow(self) -> str:
        return """\
void
fp_pow_pubexp(fp_t *out, const fp_t *a, const uint32_t e[FP_LIMBS])
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
        uint32_t ew = e[i];
        for (unsigned j = 8; j-- > 0;) {
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
    for (size_t i = 0; i < FP_LIMBS; i++)
        y.limb[i] &= ok;

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

    def generate_binary_gcd_helpers(self) -> str:
        return """\
static inline void
fp_montylin(fp_t *out, const fp_t *u, const fp_t *v, uint32_t f, uint32_t g)
{
    uint32_t sf = sign_word(f);
    uint32_t af = (f ^ sf) - sf;
    uint32_t sg = sign_word(g);
    uint32_t ag = (g ^ sg) - sg;
    fp_t tu = *u, tv = *v, neg;
    fp_neg(&neg, u);
    for (size_t i = 0; i < FP_LIMBS; i++)
        tu.limb[i] ^= sf & (tu.limb[i] ^ neg.limb[i]);
    fp_neg(&neg, v);
    for (size_t i = 0; i < FP_LIMBS; i++)
        tv.limb[i] ^= sg & (tv.limb[i] ^ neg.limb[i]);

    uint32_t lo, cc;
    mul_x2(&lo, &cc, tu.limb[0], af, tv.limb[0], ag);
    out->limb[0] = lo;
    for (size_t i = 1; i < FP_LIMBS; i++) {
        uint32_t hi;
        mul_x2_add(&lo, &hi, tu.limb[i], af, tv.limb[i], ag, cc);
        out->limb[i] = lo;
        cc = hi;
    }
    uint32_t up = cc;

    uint32_t q = __P0I_EXPR__;
    uint32_t cc2;
    mul_add(&lo, &cc2, q, FP_MODULUS[0], out->limb[0]);
    for (size_t i = 1; i < FP_LIMBS; i++) {
        uint32_t hi;
        mul_add2(&lo, &hi, q, FP_MODULUS[i], out->limb[i], cc2);
        out->limb[i - 1] = lo;
        cc2 = hi;
    }
    unsigned char cc1 = addcarry_u32(0, up, cc2,
                                               &out->limb[FP_LIMBS - 1]);

    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        borrow = subborrow_u32(borrow, out->limb[i], FP_MODULUS[i], &out->limb[i]);

    uint32_t mask = (uint32_t)cc1 - (uint32_t)borrow;
    unsigned char carry = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        carry = addcarry_u32(carry, out->limb[i], FP_MODULUS[i] & mask, &out->limb[i]);
    (void)carry;
}

static inline uint32_t
fp_lindiv15abs(fp_t *out, const fp_t *a, const fp_t *b, uint32_t f, uint32_t g)
{
    uint32_t sf = sign_word(f);
    uint32_t af = (f ^ sf) - sf;
    uint32_t sg = sign_word(g);
    uint32_t ag = (g ^ sg) - sg;
    unsigned char cc1 = 0, cc2 = 0;
    uint32_t cc3 = 0;

    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint32_t aa, bb, d, hi;
        cc1 = subborrow_u32(cc1, a->limb[i] ^ sf, sf, &aa);
        cc2 = subborrow_u32(cc2, b->limb[i] ^ sg, sg, &bb);
        mul_x2_add(&d, &hi, aa, af, bb, ag, cc3);
        out->limb[i] = d;
        cc3 = hi;
    }

    uint32_t up = cc3 - (((uint32_t)0 - (uint32_t)cc1) & af)
                    - (((uint32_t)0 - (uint32_t)cc2) & ag);
    for (size_t i = 0; i + 1 < FP_LIMBS; i++)
        out->limb[i] = (out->limb[i] >> 15) | (out->limb[i + 1] << 17);
    out->limb[FP_LIMBS - 1] = (out->limb[FP_LIMBS - 1] >> 15) | (up << 17);

    uint32_t w = sign_word(up);
    unsigned char borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        borrow = subborrow_u32(borrow, out->limb[i] ^ w, w, &out->limb[i]);
    (void)borrow;
    return w;
}
""".replace("__P0I_EXPR__", self.p0i_expr("out->limb[0]"))

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
        uint32_t c_hi = UINT32_MAX, c_lo = UINT32_MAX;
        uint32_t a_hi = 0, a_lo = 0, b_hi = 0, b_lo = 0;
        for (size_t j = FP_LIMBS; j-- > 0;) {
            uint32_t aw = a.limb[j], bw = b.limb[j];
            a_hi ^= (a_hi ^ aw) & c_hi;
            a_lo ^= (a_lo ^ aw) & c_lo;
            b_hi ^= (b_hi ^ bw) & c_hi;
            b_lo ^= (b_lo ^ bw) & c_lo;
            c_lo = c_hi;
            uint32_t mw = aw | bw;
            c_hi &= ((mw | (uint32_t)(0 - mw)) >> 31) - 1;
        }

        unsigned s = (unsigned)__builtin_clz(a_hi | b_hi);
        uint32_t xa = (a_hi << s) | ((a_lo >> 1) >> (31 - s));
        uint32_t xb = (b_hi << s) | ((b_lo >> 1) >> (31 - s));
        xa = (xa & UINT32_C(0xffff8000)) | (a.limb[0] & UINT32_C(0x00007fff));
        xb = (xb & UINT32_C(0xffff8000)) | (b.limb[0] & UINT32_C(0x00007fff));
        xa ^= c_lo & (xa ^ a.limb[0]);
        xb ^= c_lo & (xb ^ b.limb[0]);

        uint32_t fg0 = 1, fg1 = UINT32_C(1) << 16;
        for (unsigned k = 0; k < 15; k++) {
            uint32_t a_odd = (uint32_t)0 - (xa & 1);
            uint32_t d;
            unsigned char borrow = subborrow_u32(0, xa, xb, &d);
            uint32_t swap = a_odd & ((uint32_t)0 - (uint32_t)borrow);
            uint32_t t = swap & (xa ^ xb); xa ^= t; xb ^= t;
            t = swap & (fg0 ^ fg1); fg0 ^= t; fg1 ^= t;
            xa -= a_odd & xb;
            fg0 -= a_odd & fg1;
            xa >>= 1;
            fg1 <<= 1;
        }
        fg0 += UINT32_C(0x7fff7fff);
        fg1 += UINT32_C(0x7fff7fff);
        uint32_t f0 = (fg0 & UINT32_C(0xffff)) - UINT32_C(0x7fff);
        uint32_t g0 = (fg0 >> 16) - UINT32_C(0x7fff);
        uint32_t f1 = (fg1 & UINT32_C(0xffff)) - UINT32_C(0x7fff);
        uint32_t g1 = (fg1 >> 16) - UINT32_C(0x7fff);

        fp_t na, nb, nu, nv;
        uint32_t nega = fp_lindiv15abs(&na, &a, &b, f0, g0);
        uint32_t negb = fp_lindiv15abs(&nb, &a, &b, f1, g1);
        f0 = (f0 ^ nega) - nega; g0 = (g0 ^ nega) - nega;
        f1 = (f1 ^ negb) - negb; g1 = (g1 ^ negb) - negb;
        fp_montylin(&nu, &u, &v, f0, g0);
        fp_montylin(&nv, &u, &v, f1, g1);
        a = na; b = nb; u = nu; v = nv;
    }

    uint32_t xa = a.limb[0], xb = b.limb[0];
    uint32_t f0 = 1, g0 = 0, f1 = 0, g1 = 1;
    for (size_t k = 0; k < FP_NUM2; k++) {
        uint32_t a_odd = (uint32_t)0 - (xa & 1);
        uint32_t d;
        unsigned char borrow = subborrow_u32(0, xa, xb, &d);
        uint32_t swap = a_odd & ((uint32_t)0 - (uint32_t)borrow);
        uint32_t t = swap & (xa ^ xb); xa ^= t; xb ^= t;
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

    uint32_t z = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) z |= x->limb[i];
    uint32_t mask = (uint32_t)0 - ((z | (uint32_t)(0 - z)) >> 31);
    for (size_t i = 0; i < FP_LIMBS; i++) out->limb[i] &= mask;
    fp_mul(out, out, &FP_TFIXDIV);
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
     * R = 2^(32*N) is a square in GF(p).
     */
    fp_t a = *x;
    fp_t b = { { 0 } };
    for (size_t i = 0; i < FP_LIMBS; i++)
        b.limb[i] = FP_MODULUS[i];
    uint32_t ls = 0;

    for (size_t outer = 0; outer < FP_NUM1; outer++) {
        uint32_t c_hi = UINT32_MAX, c_lo = UINT32_MAX;
        uint32_t a_hi = 0, a_lo = 0, b_hi = 0, b_lo = 0;

        for (size_t j = FP_LIMBS; j-- > 0;) {
            uint32_t aw = a.limb[j], bw = b.limb[j];
            a_hi ^= (a_hi ^ aw) & c_hi;
            a_lo ^= (a_lo ^ aw) & c_lo;
            b_hi ^= (b_hi ^ bw) & c_hi;
            b_lo ^= (b_lo ^ bw) & c_lo;
            c_lo = c_hi;
            uint32_t mw = aw | bw;
            c_hi &= ((mw | (uint32_t)(0 - mw)) >> 31) - 1;
        }

        unsigned s = (unsigned)__builtin_clz(a_hi | b_hi);
        uint32_t xa = (a_hi << s) | ((a_lo >> 1) >> (31 - s));
        uint32_t xb = (b_hi << s) | ((b_lo >> 1) >> (31 - s));
        xa = (xa & UINT32_C(0xffff8000)) |
             (a.limb[0] & UINT32_C(0x00007fff));
        xb = (xb & UINT32_C(0xffff8000)) |
             (b.limb[0] & UINT32_C(0x00007fff));
        xa ^= c_lo & (xa ^ a.limb[0]);
        xb ^= c_lo & (xb ^ b.limb[0]);

        /* First 13 inner iterations. */
        uint32_t fg0 = 1, fg1 = UINT32_C(1) << 16;
        for (unsigned k = 0; k < 13; k++) {
            uint32_t a_odd = (uint32_t)0 - (xa & 1);
            uint32_t d;
            unsigned char borrow = subborrow_u32(0, xa, xb, &d);
            uint32_t swap = a_odd & ((uint32_t)0 - (uint32_t)borrow);

            ls ^= swap & ((xa & xb) >> 1);

            uint32_t t = swap & (xa ^ xb);
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
        uint32_t fg0z = fg0 + UINT32_C(0x7fff7fff);
        uint32_t fg1z = fg1 + UINT32_C(0x7fff7fff);
        uint32_t f0 = (fg0z & UINT32_C(0xffff)) - UINT32_C(0x7fff);
        uint32_t g0 = (fg0z >> 16) - UINT32_C(0x7fff);
        uint32_t f1 = (fg1z & UINT32_C(0xffff)) - UINT32_C(0x7fff);
        uint32_t g1 = (fg1z >> 16) - UINT32_C(0x7fff);
        uint32_t a0 = (a.limb[0] * f0 + b.limb[0] * g0) >> 13;
        uint32_t b0 = (a.limb[0] * f1 + b.limb[0] * g1) >> 13;

        for (unsigned k = 0; k < 2; k++) {
            uint32_t a_odd = (uint32_t)0 - (xa & 1);
            uint32_t d;
            unsigned char borrow = subborrow_u32(0, xa, xb, &d);
            uint32_t swap = a_odd & ((uint32_t)0 - (uint32_t)borrow);

            ls ^= swap & ((a0 & b0) >> 1);

            uint32_t t = swap & (xa ^ xb);
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

        /* Propagate the 15-step transformation to the full operands. */
        fg0 += UINT32_C(0x7fff7fff);
        fg1 += UINT32_C(0x7fff7fff);
        f0 = (fg0 & UINT32_C(0xffff)) - UINT32_C(0x7fff);
        g0 = (fg0 >> 16) - UINT32_C(0x7fff);
        f1 = (fg1 & UINT32_C(0xffff)) - UINT32_C(0x7fff);
        g1 = (fg1 >> 16) - UINT32_C(0x7fff);

        fp_t na, nb;
        uint32_t nega = fp_lindiv15abs(&na, &a, &b, f0, g0);
        (void)fp_lindiv15abs(&nb, &a, &b, f1, g1);
        ls ^= nega & (nb.limb[0] >> 1);
        a = na;
        b = nb;
    }

    /* The remaining operands fit in one word. */
    uint32_t xa = a.limb[0];
    uint32_t xb = b.limb[0];
    for (size_t k = 0; k < FP_NUM2; k++) {
        uint32_t a_odd = (uint32_t)0 - (xa & 1);
        uint32_t d;
        unsigned char borrow = subborrow_u32(0, xa, xb, &d);
        uint32_t swap = a_odd & ((uint32_t)0 - (uint32_t)borrow);

        ls ^= swap & ((xa & xb) >> 1);

        uint32_t t = swap & (xa ^ xb);
        xa ^= t;
        xb ^= t;
        xa -= a_odd & xb;
        xa >>= 1;
        ls ^= (xb + 2) >> 2;
    }

    /* 0 -> 0, QR -> +1, QNR -> -1. */
    uint32_t r = 1u - ((ls & 1u) << 1);
    uint32_t z = 0;
    for (size_t i = 0; i < FP_LIMBS; i++)
        z |= x->limb[i];
    uint32_t nz = (uint32_t)((z | (uint32_t)(0 - z)) >> 31);
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
        uint32_t w = 0;
        for (size_t j = 0; j < 4; j++)
            w |= (uint32_t)in[4 * i + j] << (8 * j);
        raw.limb[i] = w;
    }

    {
        const size_t last = FP_LIMBS - 1;
        const size_t last_bytes = FP_ENCODED_BYTES - 4 * (FP_LIMBS - 1);
        uint32_t w = 0;
        for (size_t j = 0; j < last_bytes; j++)
            w |= (uint32_t)in[4 * last + j] << (8 * j);
        raw.limb[last] = w;
    }

    *out = raw;
}

uint32_t
fp_decode(fp_t *out, const uint8_t in[FP_ENCODED_BYTES])
{
    fp_t raw;
    fp_decode_nocheck(&raw, in);

    uint32_t borrow = 0;
    for (size_t i = 0; i < FP_LIMBS; i++) {
        uint32_t d;
        borrow = subborrow_u32((unsigned char)borrow,
                                       raw.limb[i], FP_MODULUS[i], &d);
    }

    uint32_t mask = (uint32_t)0 - borrow;
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
        uint32_t w = t.limb[i];
        for (size_t j = 0; j < 4; j++) {
            out[4 * i + j] = (uint8_t)(w >> (8 * j));
        }
    }

    {
        const size_t last = FP_LIMBS - 1;
        const size_t last_bytes = FP_ENCODED_BYTES - 4 * (FP_LIMBS - 1);
        uint32_t w = t.limb[last];

        for (size_t j = 0; j < last_bytes; j++) {
            out[4 * last + j] = (uint8_t)(w >> (8 * j));
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
            self.generate_hadamard(),
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
            self.generate_vartime_pow(),
            self.generate_binary_gcd_helpers(),
            self.generate_inv(),
            self.generate_legendre(),
            self.generate_is_square(),
            self.generate_batch_inversion(),
            self.generate_sqrt(),
            self.generate_exp3div4(),
            self.generate_decode(),
            self.generate_decode_reduce(),
            self.generate_encode(),
        ]
        return '#include "fp.h"\n#include "util_32.h"\n\n#include <stddef.h>\n#include <stdint.h>\n#include <string.h>\n\n' + '\n'.join(methods)

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

    FieldGeneratorU32(parse_int(args.prime)).generate(
        Path(args.include_dir), Path(args.source_dir)
    )


if __name__ == "__main__":
    main()
