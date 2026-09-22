#!/usr/bin/env python3
"""
Generator of x86-64 Broadwell (MULX/ADCX/ADOX) assembly for fp_mul,
fp_sqr, fp_sum_of_products and fp_difference_of_products, for our
Montgomery-friendly primes p = c * 2^t - 1.

Matches generator/gen_fp.py's fp_t representation exactly (radix 2^64,
N = ceil(bits/64) limbs, R = 2^(64*N) mod p) and produces a fully
canonical, drop-in replacement: unlike the SQIsign submission's own asm
generator (gen_fp_from_sqisign/), which leaves output lazily reduced in
[0, 2^bits) and needs spare top-limb bits to stay bounded, this always
finishes with one full conditional subtraction of p. Standard Montgomery
CIOS theory guarantees that's enough (the raw accumulator is always < 2p
for any prime), so this works regardless of spare bits.

Because these primes have p = c*2^t - 1 with t >= 64*(N-1), the low N-1
limbs of p are all 0xFFFFFFFFFFFFFFFF, forcing mu = -p^-1 mod 2^64 = 1
(a row's own low limb *is* the reduction multiplier) and making p+1 have
a single non-zero 64-bit limb (folding in "m * p" collapses from an
N-limb multiply-add to one 64x64 multiply). fp_mul is row-interleaved
CIOS: one schoolbook row (paired MULX/ADCX/ADOX so the two carry chains
run independently) immediately followed by that one-multiply reduction
fold, keeping the accumulator to N+1 limbs throughout.

Usage:
    python gen_fp_asm_broadwell.py --prime "633 * 2**308 - 1" -o fp_asm.S
"""

import argparse
from pathlib import Path

MASK64 = (1 << 64) - 1


class AsmParams:
    def __init__(self, p: int):
        self.p = p
        self.bits = p.bit_length()
        self.n = (self.bits + 63) // 64
        if self.n < 2:
            raise ValueError("primes below 2 limbs are not supported")

        self.modulus = [(p >> (64 * i)) & MASK64 for i in range(self.n)]
        if self.modulus[0] != MASK64:
            raise ValueError(
                "this generator requires mu = -p^-1 mod 2^64 == 1, i.e. the "
                "low limb of p must be 0xFFFFFFFFFFFFFFFF (true for every "
                "p = c*2^t - 1 with t >= 64*(n-1)); got low limb "
                "0x%016x" % self.modulus[0]
            )

        # p + 1 = c * 2^t must have exactly one non-zero 64-bit limb.
        p_plus_1 = p + 1
        top_idx = self.n - 1
        if any(((p_plus_1 >> (64 * i)) & MASK64) != 0 for i in range(top_idx)):
            raise ValueError("p + 1 must have a single non-zero 64-bit limb "
                             "(the top one): this generator only targets "
                             "p = c*2^t - 1 primes with t >= 64*(n-1)")
        p_plus_1_top = p_plus_1 >> (64 * top_idx)
        if p_plus_1_top.bit_length() > 64:
            raise ValueError(
                "p+1's non-zero limb is %d bits: the reduction fold's single "
                "mulx needs it to fit in 64 bits (got 0x%x)"
                % (p_plus_1_top.bit_length(), p_plus_1_top)
            )
        self.top_idx = top_idx
        self.p_plus_1_top = p_plus_1_top


def hexq(v: int) -> str:
    return "0x%016X" % (v & MASK64)


# --------------------------------------------------------------------------
# Row-interleaved CIOS multiply
# --------------------------------------------------------------------------
def emit_row(window, b_ptr, n, t0, t1, clear_top=True):
    """window[0..n-1] += a_i * b[0..n-1] (rdx = a_i already), carry into
    window[n]. Paired ADCX/ADOX: term j's low half joins the ADCX chain
    into window[j] (the very first term has nothing to chain off, so it
    goes via ADOX instead) and its high half joins the ADOX chain into
    window[j+1]; one final ADC merges the two chains.

    clear_top: window[n] is the freshly-rotated-in slot and gets zeroed
    here (also clearing CF/OF) before use. Pass False to instead
    accumulate onto an existing window[n] -- e.g. calling this twice into
    the same window for fused sum-of-products -- while still clearing
    CF/OF via a scratch register for this call's own j=0 term.
    """
    lines = [f"xor    {window[n]}, {window[n]}"] if clear_top else [f"xor    {t0}, {t0}"]
    for j in range(n):
        b_off = "[%s]" % b_ptr if j == 0 else "[%s+%d]" % (b_ptr, 8 * j)
        lines.append(f"mulx   {t0}, {t1}, {b_off}")
        if j == 0:
            lines.append(f"adox   {window[0]}, {t1}")
            lines.append(f"adox   {window[1]}, {t0}")
        else:
            lines.append(f"adcx   {window[j]}, {t1}")
            lines.append(f"adox   {window[j + 1]}, {t0}")
    lines.append(f"adc    {window[n]}, 0")
    return lines


def emit_reduction_fold(window, n, top_idx, t0, t1):
    """m = window[0] (mu=1); fold m * (p+1)'s one non-zero limb into
    window[n-1..n] (always the top two slots, since top_idx == n-1)."""
    assert top_idx == n - 1
    return [
        f"mov    rdx, {window[0]}",
        f"mulx   {t0}, {t1}, [rip + FP_P_PLUS_1_TOP]",
        f"add    {window[n - 1]}, {t1}",
        f"adc    {window[n]}, {t0}",
    ]


# --------------------------------------------------------------------------
# Final canonicalization: one conditional subtraction of p
# --------------------------------------------------------------------------
def emit_final_reduce(result_regs, n, mask, tmp, frame_base):
    """result_regs holds a value < 2p (standard Montgomery CIOS bound).
    Subtract p; if that borrows, add it back (branch-free), mirroring
    gen_fp.py's own fp_sub/fp_add correction. The n masked-p limbs are
    staged at frame_base first: AND clobbers flags, so every masked limb
    must be ready before the add/adc chain starts -- interleaving
    "mov+and" between chain steps silently eats the next adc's carry.
    """
    lines = []
    for i in range(n):
        op = "sub" if i == 0 else "sbb"
        lines.append(f"{op}    {result_regs[i]}, [rip + FP_MODULUS + {8 * i}]")
    lines.append(f"sbb    {mask}, {mask}")  # mask = 0 - CF: all-ones if we must add p back
    for i in range(n):
        lines.append(f"mov    {tmp}, [rip + FP_MODULUS + {8 * i}]")
        lines.append(f"and    {tmp}, {mask}")
        lines.append(f"mov    [{frame_base}+{8 * i}], {tmp}")
    for i in range(n):
        op = "add" if i == 0 else "adc"
        lines.append(f"{op}    {result_regs[i]}, [{frame_base}+{8 * i}]")
    return lines


# --------------------------------------------------------------------------
# Dedicated squaring: full 2n-limb product (cross terms doubled, plus
# diagonal squares), then the fp_mul reduction fold applied n times.
# Currently generated but NOT wired up as the active fp_sqr (see
# choose_sqr_registers): it needs the whole 2n-limb product resident in
# registers at once (no row-sized window to interleave reduction into,
# unlike fp_mul), and even with that it hasn't beaten the plain
# fp_mul(a, a) alias in practice, so the alias is what's actually used.
# Kept here, generated but unreferenced, for future tuning.
# --------------------------------------------------------------------------
def emit_sqr_body(P: "AsmParams", T, hi, lo) -> list:
    n = P.n
    lines = []

    lines.append("// zero the 2n-limb product")
    for r in T:
        lines.append(f"xor    {r}, {r}")
    lines.append("")

    lines.append("// cross terms a_i*a_j (i<j), each covering both a_i*a_j and a_j*a_i.")
    lines.append("// Visited in *position* order (pos=i+j increasing): T[pos+2] is then")
    lines.append('// always the top slot touched so far, so "adc T[pos+2], 0" is safe.')
    lines.append('// A position\'s first pair still needs a plain "add" into T[pos]: that')
    lines.append("// slot can already hold a contribution from position pos-1's pairs.")
    for pos in range(1, 2 * n - 2):
        lo_i = max(0, pos - (n - 1))
        hi_i = (pos - 1) // 2
        first = True
        for i in range(lo_i, hi_i + 1):
            j = pos - i
            if not (i < j <= n - 1):
                continue
            lines.append(f"mov    rdx, [rsi + {8 * i}]" if i else "mov    rdx, [rsi]")
            lines.append(f"mulx   {hi}, {lo}, [rsi + {8 * j}]")
            op = "add" if first else "adc"
            lines.append(f"{op}    {T[pos]}, {lo}")
            lines.append(f"adc    {T[pos + 1]}, {hi}")
            lines.append(f"adc    {T[pos + 2]}, 0")
            first = False
    lines.append("")

    lines.append("// double (top-down shld: each limb keeps its pre-shift value available")
    lines.append("// for the next one up to read its top bit)")
    for k in range(2 * n - 1, 0, -1):
        lines.append(f"shld   {T[k]}, {T[k - 1]}, 1")
    lines.append(f"add    {T[0]}, {T[0]}")
    lines.append("")

    lines.append("// add diagonal squares a_i^2, one continuous carry chain")
    for i in range(n):
        lines.append(f"mov    rdx, [rsi + {8 * i}]" if i else "mov    rdx, [rsi]")
        lines.append(f"mulx   {hi}, {lo}, rdx")
        op = "add" if i == 0 else "adc"
        lines.append(f"{op}    {T[2 * i]}, {lo}")
        lines.append(f"adc    {T[2 * i + 1]}, {hi}")
    lines.append("")

    lines.append("// reduce (fp_mul's fold, applied n times). T[] is already fully")
    lines.append("// populated here, not freshly zeroed, so unlike the cross terms above a")
    lines.append("// carry can genuinely need to propagate past one extra limb.")
    for row in range(n):
        pos = row + P.top_idx
        lines.append(f"mov    rdx, {T[row]}")
        lines.append(f"mulx   {hi}, {lo}, [rip + FP_P_PLUS_1_TOP]")
        lines.append(f"add    {T[pos]}, {lo}")
        lines.append(f"adc    {T[pos + 1]}, {hi}")
        for k in range(pos + 2, 2 * n):
            lines.append(f"adc    {T[k]}, 0")
    lines.append("")

    return lines


# --------------------------------------------------------------------------
# Register pools
# --------------------------------------------------------------------------
# fp_mul needs n+3 registers live at once (n+1-limb window + 2 mul temps).
CALLER_SAVED_POOL = ["r8", "r9", "r10", "r11", "rax"]
CALLEE_SAVED_POOL = ["rbx", "r12", "r13", "r14", "r15", "rbp"]

# Past 11 registers, free up pointer registers one at a time: rcx first
# (copy b to a stack buffer once, so its row reads become [rsp+...]
# instead of [rcx+...] and rcx itself becomes another accumulator), then
# rdi (park the output pointer in a stack slot, reload only for the final
# store). rsi (a's pointer) is read every row and is never spilled.
SPILLABLE_POINTER_REGS = ["rcx", "rdi"]

MAX_LIMBS_REGS = len(CALLER_SAVED_POOL) + len(CALLEE_SAVED_POOL) - 3
MAX_LIMBS_SPILL = MAX_LIMBS_REGS + len(SPILLABLE_POINTER_REGS)

# Squaring has no b pointer, so rcx is a bonus register with no spilling.
SQR_POOL = CALLER_SAVED_POOL + ["rcx"] + CALLEE_SAVED_POOL
MAX_LIMBS_SQR = (len(SQR_POOL) - 2) // 2


def choose_sqr_registers(n: int):
    """Register plan for the dedicated squaring routine, or None if n is
    too large to hold the full 2n-limb product plus two mulx temporaries."""
    needed = 2 * n + 2
    if needed > len(SQR_POOL):
        return None
    regs = SQR_POOL[:needed]
    product_regs = regs[: 2 * n]
    hi, lo = regs[2 * n], regs[2 * n + 1]
    to_save = [r for r in CALLEE_SAVED_POOL if r in regs]
    return product_regs, hi, lo, to_save


def choose_registers(n: int):
    """Register-resident if n+3 fits in 11 registers, otherwise spill
    pointer registers one at a time (SPILLABLE_POINTER_REGS) until it
    does. Returns (window_regs, t0, t1, to_save, spill)."""
    base_pool = CALLER_SAVED_POOL + CALLEE_SAVED_POOL
    needed = n + 3
    if needed <= len(base_pool):
        spill = []
    else:
        extra = needed - len(base_pool)
        if extra > len(SPILLABLE_POINTER_REGS):
            raise ValueError(
                "p needs %d limbs: this generator tops out at %d limbs "
                "(register-resident up to %d limbs, pointer-spilling up "
                "to %d; beyond that 'a' would need spilling too, which "
                "isn't implemented)" % (n, MAX_LIMBS_SPILL, MAX_LIMBS_REGS, MAX_LIMBS_SPILL)
            )
        spill = SPILLABLE_POINTER_REGS[:extra]

    pool = CALLER_SAVED_POOL + spill + CALLEE_SAVED_POOL
    regs = pool[:needed]
    window_regs = regs[: n + 1]
    t0, t1 = regs[n + 1], regs[n + 2]
    to_save = [r for r in CALLEE_SAVED_POOL if r in regs]
    return window_regs, t0, t1, to_save, spill


# --------------------------------------------------------------------------
# Fused sum/difference of products: r = a1*b1 (+/-) a2*b2, what fp2_mul's
# real/imaginary parts need. Each CIOS row does two schoolbook
# multiply-adds (a1[i]*b1[..] then a2[i]*b2[..]) into the same window
# before a single reduction fold: half the reduction work of two separate
# fp_mul calls plus an add/sub. Difference reuses the same core by
# negating b2 once up front (same technique as fp_neg) into a stack
# buffer.
#
# 5 pointer args (r, a1, b1, a2, b2) means tighter register pressure than
# fp_mul: r is always parked on the stack (like fp_mul's spill_rdi, but
# unconditional here), freeing rdi to join the window/temp pool; b1
# arrives in rdx and moves to r9 immediately since rdx is needed for
# mulx's implicit operand.
# --------------------------------------------------------------------------
SOP_POOL = ["rax", "r10", "r11", "r12", "r13", "r14", "r15", "rdi", "rbx", "rbp"]
MAX_LIMBS_SOP = len(SOP_POOL) - 3


def choose_sop_registers(n: int):
    """Register plan for fused sum/difference of products, or None if n
    doesn't fit (caller falls back to two fp_mul calls plus an add/sub)."""
    needed = n + 3
    if needed > len(SOP_POOL):
        return None
    regs = SOP_POOL[:needed]
    window_regs = regs[: n + 1]
    t0, t1 = regs[n + 1], regs[n + 2]
    to_save = [r for r in CALLEE_SAVED_POOL if r in regs]
    return window_regs, t0, t1, to_save


def emit_negate_to_stack(n, src_ptr, dst_off, stage_off, t0, t1):
    """[rsp+dst_off] = 0 - [src_ptr] (mod p): same subborrow-chain +
    masked-add-back as portable C's fp_neg. Masked-p limbs are staged at
    stage_off first (see emit_final_reduce for why: AND clobbers flags)."""
    lines = ["clc"]
    for i in range(n):
        lines.append(f"mov    {t0}, 0")
        src = f"[{src_ptr}]" if i == 0 else f"[{src_ptr} + {8 * i}]"
        lines.append(f"sbb    {t0}, {src}")
        lines.append(f"mov    [rsp+{dst_off + 8 * i}], {t0}")
    lines.append(f"sbb    {t1}, {t1}")  # mask = 0 - final borrow
    for i in range(n):
        lines.append(f"mov    {t0}, [rip + FP_MODULUS + {8 * i}]")
        lines.append(f"and    {t0}, {t1}")
        lines.append(f"mov    [rsp+{stage_off + 8 * i}], {t0}")
    for i in range(n):
        lines.append(f"mov    {t0}, [rsp+{stage_off + 8 * i}]")
        op = "add" if i == 0 else "adc"
        lines.append(f"{op}    [rsp+{dst_off + 8 * i}], {t0}")
    return lines


def emit_sop_core(P: "AsmParams", window_regs, t0, t1, a1_ptr, b1_ptr, a2_ptr, b2_ptr):
    n = P.n
    lines = ["// zero the window (see fp_mul for why)"]
    for r in window_regs:
        lines.append(f"xor    {r}, {r}")
    lines.append("")
    for i in range(n):
        window = [window_regs[(i + k) % (n + 1)] for k in range(n + 1)]
        lines.append(f"// row {i}: window += a1[{i}]*b1[..] + a2[{i}]*b2[..], then reduce")
        lines.append(f"mov    rdx, [{a1_ptr} + {8 * i}]" if i else f"mov    rdx, [{a1_ptr}]")
        lines.extend(emit_row(window, b1_ptr, n, t0, t1))
        lines.append(f"mov    rdx, [{a2_ptr} + {8 * i}]" if i else f"mov    rdx, [{a2_ptr}]")
        lines.extend(emit_row(window, b2_ptr, n, t0, t1, clear_top=False))
        lines.extend(emit_reduction_fold(window, n, P.top_idx, t0, t1))
        lines.append("")
    final_window = [window_regs[(n - 1 + k) % (n + 1)] for k in range(n + 1)]
    result_regs = final_window[1:]  # final_window[0] is stale, dropped
    return lines, result_regs


def emit_sop_routine(P: "AsmParams", name: str, difference: bool) -> list:
    n = P.n
    window_regs, t0, t1, to_save = choose_sop_registers(n)
    reduce_off = 16
    neg_b2_off = reduce_off + ((n * 8 + 15) // 16) * 16
    neg_stage_off = neg_b2_off + ((n * 8 + 15) // 16) * 16
    frame_size = neg_stage_off + (((n * 8 + 15) // 16) * 16 if difference else 0)

    lines = [f".global CDECL({name})", f"CDECL({name}):"]
    sign = "-" if difference else "+"
    lines.append(f"// void {name}(fp_t *r, const fp_t *a1, const fp_t *b1,")
    lines.append(f"//                const fp_t *a2, const fp_t *b2);  r = a1*b1 {sign} a2*b2")
    lines.append("// SysV: rdi=r, rsi=a1, rdx=b1, rcx=a2, r8=b2.")
    for r in to_save:
        lines.append(f"push   {r}")
    lines.append(f"sub    rsp, {frame_size}")
    lines.append("mov    [rsp], rdi  // r has no dedicated register here; park it")
    lines.append("mov    r9, rdx     // free up rdx for mulx's implicit operand; r9 = b1")
    lines.append("")

    if difference:
        lines.append("// b2 <- -b2, then treat it as an ordinary operand (reuses the sum core)")
        lines.extend(emit_negate_to_stack(n, "r8", neg_b2_off, neg_stage_off, t0, t1))
        lines.append("")
        b2_ptr = f"rsp+{neg_b2_off}"
    else:
        b2_ptr = "r8"

    core_lines, result_regs = emit_sop_core(P, window_regs, t0, t1, "rsi", "r9", "rcx", b2_ptr)
    lines.extend(core_lines)
    lines.append("// canonicalize: result is < 2p; subtract p once if needed")
    lines.extend(emit_final_reduce(result_regs, n, t0, t1, f"rsp+{reduce_off}"))
    lines.append("")
    lines.append(f"mov    {t0}, [rsp]  // reload r")
    for i in range(n):
        lines.append(f"mov    [{t0} + {8 * i}], {result_regs[i]}" if i else f"mov    [{t0}], {result_regs[i]}")
    lines.append("")
    lines.append(f"add    rsp, {frame_size}")
    for r in reversed(to_save):
        lines.append(f"pop    {r}")
    lines.append("ret")
    lines.append("")
    return lines


# --------------------------------------------------------------------------
# Hadamard: r1 = a+b, r2 = a-b, both mod p, from a single pass over a and b.
#
# Plain ADC/SBB share one flag (CF), so a compiler computing both an add
# chain and a subtract chain per limb has to run them in two separate
# passes -- which is what clang -O3 does for the portable-C fp_hadamard,
# forcing it to spill a[]/b[] across the gap. ADCX/ADOX give two
# *independent* carry flags (CF, OF), so both chains run truly
# interleaved here: the add chain uses CF as normal, and the subtract is
# computed as two's-complement addition (a + ~b + 1) on the OF chain,
# seeded with OF=1 up front so the "+1" is free. The classic identity
# applies: the final OF is 1 iff a >= b (no borrow), 0 iff a < b.
#
# Raw (unreduced) limbs are stored out to r1[]/r2[] as they're computed,
# not kept resident, so this needs only 3 rotating temporaries regardless
# of limb count -- no spilling, unlike fp_mul/fp_sqr at high limb counts.
#
# Benchmarked and found NOT to beat the portable-C fused fp_hadamard (see
# generator/gen_fp.py's generate_hadamard): correct (1M+ random trials,
# all three primes, zero mismatches) but consistently slower, both in
# isolation and inside dim_four_hadamard's 32 calls per transform. The
# store-then-reload round trip through r1[]/r2[] for both reduction
# passes, plus fixed per-call overhead (prologue, the CF/OF seed trick,
# mask extraction), apparently outweighs the win from true CF/OF
# interleaving. Not wired up as fp_hadamard -- emitted under
# fp_hadamard_dedicated instead, generated but unreferenced, for future
# tuning (same treatment as fp_sqr_dedicated above).
# --------------------------------------------------------------------------
HADAMARD_POOL = ["rax", "rbx", "rdx", "rcx", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rbp"]
MAX_LIMBS_HADAMARD = len(HADAMARD_POOL) - 2


def choose_hadamard_registers(n: int):
    """Register plan for the reduction phases (main loop always fits: it
    only ever needs 3 rotating temporaries). None if n is too large."""
    needed = n + 2
    if needed > len(HADAMARD_POOL):
        return None
    regs = HADAMARD_POOL[:needed]
    window_regs = regs[:n]
    mask_reg, tmp_reg = regs[n], regs[n + 1]
    to_save = [r for r in CALLEE_SAVED_POOL if r in regs]
    return window_regs, mask_reg, tmp_reg, to_save


def emit_hadamard_core(n, r1_ptr, r2_ptr, a_ptr, b_ptr, t0, t1, t2):
    lines = []
    lines.append("// seed CF=0 (sum chain) and OF=1 (diff chain's two's-complement +1):")
    lines.append("// al=1, then al+0x7f=0x80 unsigned-fits (CF=0) but signed-overflows (OF=1)")
    lines.append("mov    eax, 1")
    lines.append("add    al, 0x7f")
    lines.append("")
    for i in range(n):
        a_off = f"[{a_ptr}]" if i == 0 else f"[{a_ptr}+{8 * i}]"
        b_off = f"[{b_ptr}]" if i == 0 else f"[{b_ptr}+{8 * i}]"
        r1_off = f"[{r1_ptr}]" if i == 0 else f"[{r1_ptr}+{8 * i}]"
        r2_off = f"[{r2_ptr}]" if i == 0 else f"[{r2_ptr}+{8 * i}]"
        lines.append(f"// limb {i}: sum on the CF chain, a + ~b on the OF chain")
        lines.append(f"mov    {t0}, {a_off}")
        lines.append(f"mov    {t1}, {b_off}")
        lines.append(f"mov    {t2}, {t0}")
        lines.append(f"adcx   {t2}, {t1}")
        lines.append(f"mov    {r1_off}, {t2}")
        lines.append(f"not    {t1}")
        lines.append(f"adox   {t0}, {t1}")
        lines.append(f"mov    {r2_off}, {t0}")
    lines.append("")
    return lines


def emit_hadamard_routine(P: "AsmParams"):
    n = P.n
    plan = choose_hadamard_registers(n)
    if plan is None:
        return None
    window_regs, mask_reg, tmp_reg, to_save = plan

    lines = [".global CDECL(fp_hadamard_dedicated)", "CDECL(fp_hadamard_dedicated):"]
    lines.append("// void fp_hadamard_dedicated(fp_t *r1, fp_t *r2, const fp_t *a, const fp_t *b);")
    lines.append("// r1 = a+b, r2 = a-b (mod p). Not currently used (see comment above);")
    lines.append("// generated for future tuning. SysV: rdi=r1, rsi=r2, rdx=a, rcx=b.")
    for r in to_save:
        lines.append(f"push   {r}")
    frame_size = ((n * 8 + 15) // 16) * 16
    lines.append(f"sub    rsp, {frame_size}  // masked-p staging, reused by both reductions")
    lines.append("")

    lines.extend(emit_hadamard_core(n, "rdi", "rsi", "rdx", "rcx", "r9", "r10", "r11"))

    lines.append("// borrow_diff = (a < b), i.e. OF == 0 from the diff chain above;")
    lines.append("// mask_diff = 0 - borrow_diff (setno/movzx/neg don't touch flags,")
    lines.append("// so this safely reads OF before anything else disturbs it)")
    lines.append("setno  r8b")
    lines.append("movzx  r8, r8b")
    lines.append("neg    r8")
    lines.append("")

    lines.append("// --- reduce r1 (sum): standard subtract-p, conditional add-back.")
    lines.append("// (a, b < p with plenty of spare top bits for these primes, so a+b")
    lines.append("// never overflows the n-limb window; no extra carry to track here.)")
    for i in range(n):
        lines.append(f"mov    {window_regs[i]}, [rdi + {8 * i}]" if i else f"mov    {window_regs[0]}, [rdi]")
    lines.extend(emit_final_reduce(window_regs, n, mask_reg, tmp_reg, "rsp"))
    for i in range(n):
        lines.append(f"mov    [rdi + {8 * i}], {window_regs[i]}" if i else f"mov    [rdi], {window_regs[0]}")
    lines.append("")

    lines.append("// --- reduce r2 (diff): already subtracted via two's complement above;")
    lines.append("// just conditionally add p back if a < b.")
    for i in range(n):
        lines.append(f"mov    {window_regs[i]}, [rsi + {8 * i}]" if i else f"mov    {window_regs[0]}, [rsi]")
    for i in range(n):
        lines.append(f"mov    {tmp_reg}, [rip + FP_MODULUS + {8 * i}]")
        lines.append(f"and    {tmp_reg}, r8")
        lines.append(f"mov    [rsp+{8 * i}], {tmp_reg}")
    for i in range(n):
        op = "add" if i == 0 else "adc"
        lines.append(f"{op}    {window_regs[i]}, [rsp+{8 * i}]")
    for i in range(n):
        lines.append(f"mov    [rsi + {8 * i}], {window_regs[i]}" if i else f"mov    [rsi], {window_regs[0]}")
    lines.append("")

    lines.append(f"add    rsp, {frame_size}")
    for r in reversed(to_save):
        lines.append(f"pop    {r}")
    lines.append("ret")
    lines.append("")
    return lines


def generate(p: int) -> str:
    P = AsmParams(p)
    n = P.n
    window_regs, t0, t1, to_save, spill = choose_registers(n)
    spill_rcx = "rcx" in spill
    spill_rdi = "rdi" in spill

    lines = []
    lines.append("// Generated by generator/gen_fp_asm_broadwell.py -- do not edit by hand.")
    lines.append("// p = 0x%x (%d bits, %d limbs)" % (p, P.bits, n))
    lines.append("")
    lines.append("#if defined(__APPLE__)")
    lines.append("#define CDECL(x) _##x")
    lines.append("#else")
    lines.append("#define CDECL(x) x")
    lines.append("#endif")
    lines.append("")
    lines.append(".intel_syntax noprefix")
    lines.append("")
    lines.append("#if defined(__APPLE__)")
    lines.append(".section __TEXT,__const")
    lines.append("#else")
    lines.append(".section .rodata")
    lines.append("#endif")
    lines.append(".p2align 3")
    lines.append("FP_MODULUS:")
    for i in range(n):
        lines.append(f"    .quad {hexq(P.modulus[i])}")
    lines.append("FP_P_PLUS_1_TOP:")
    lines.append(f"    .quad {hexq(P.p_plus_1_top)}")
    lines.append("")
    lines.append(".text")
    lines.append(".p2align 4")
    lines.append(".global CDECL(fp_mul)")
    lines.append("CDECL(fp_mul):")
    lines.append("// void fp_mul(fp_t *r, const fp_t *a, const fp_t *b);")
    lines.append("// SysV: rdi = r, rsi = a, rdx = b.")
    # Stack layout when spilling: final-reduce staging, then (if rcx was
    # spilled) a buffer holding all of b, then (if rdi was spilled) one
    # slot for the output pointer.
    frame_reduce = ((n * 8 + 15) // 16) * 16
    b_buf_off = frame_reduce
    b_buf_size = n * 8 if spill_rcx else 0
    rdi_slot_off = frame_reduce + b_buf_size
    frame_size = ((frame_reduce + b_buf_size + (8 if spill_rdi else 0) + 15) // 16) * 16

    for r in to_save:
        lines.append(f"push   {r}")
    if frame_size:
        note = ["final-reduce staging"]
        if spill_rcx:
            note.append("b buffer")
        if spill_rdi:
            note.append("saved output pointer")
        lines.append(f"sub    rsp, {frame_size}  // {', '.join(note)}")

    if spill_rcx:
        lines.append(f"// p needs {n} limbs: b doesn't fit in its own pointer register here,")
        lines.append("// so copy it to the stack once and read every row from [rsp+...].")
        for j in range(n):
            src = "[rdx]" if j == 0 else f"[rdx+{8 * j}]"
            lines.append(f"mov    {t0}, {src}")
            lines.append(f"mov    [rsp+{b_buf_off + 8 * j}], {t0}")
        b_ptr = f"rsp+{b_buf_off}"
    else:
        lines.append("mov    rcx, rdx  // free up rdx for mulx's implicit operand; rcx = b")
        b_ptr = "rcx"

    if spill_rdi:
        lines.append(f"mov    [rsp+{rdi_slot_off}], rdi  // output pointer doesn't fit either; park it")

    lines.append('// zero the window before row 0: each row\'s own "xor window[n],')
    lines.append('// window[n]" only clears the freshly-rotated-in slot, not the whole')
    lines.append("// window, so row 0 needs this done explicitly first.")
    for r in window_regs:
        lines.append(f"xor    {r}, {r}")
    lines.append("")
    for i in range(n):
        window = [window_regs[(i + k) % (n + 1)] for k in range(n + 1)]
        lines.append(f"// row {i}: window[0..{n}] += a[{i}] * b[0..{n - 1}], then reduce")
        lines.append(f"mov    rdx, [rsi + {8 * i}]" if i else "mov    rdx, [rsi]")
        lines.extend(emit_row(window, b_ptr, n, t0, t1))
        lines.extend(emit_reduction_fold(window, n, P.top_idx, t0, t1))
        lines.append("")
    # window[0] of the last row is stale (CIOS guarantees it's
    # mathematically zero) and simply dropped.
    final_window = [window_regs[(n - 1 + k) % (n + 1)] for k in range(n + 1)]
    result_regs = final_window[1:]
    dropped_reg = final_window[0]
    lines.append("// canonicalize: result is < 2p; subtract p once if needed")
    lines.extend(emit_final_reduce(result_regs, n, t0, t1, "rsp"))
    lines.append("")
    if spill_rdi:
        store_ptr = dropped_reg
        lines.append(f"mov    {store_ptr}, [rsp+{rdi_slot_off}]  // reload the output pointer")
    else:
        store_ptr = "rdi"
    for i in range(n):
        lines.append(f"mov    [{store_ptr} + {8 * i}], {result_regs[i]}" if i else f"mov    [{store_ptr}], {result_regs[i]}")
    lines.append("")
    if frame_size:
        lines.append(f"add    rsp, {frame_size}")
    for r in reversed(to_save):
        lines.append(f"pop    {r}")
    lines.append("ret")
    lines.append("")

    # fp_sqr: the dedicated routine hasn't beaten the plain fp_mul(a, a)
    # alias in practice (see emit_sqr_body), so the alias is what's
    # actually wired up; the dedicated routine is still emitted below
    # under its own label, generated but unreferenced, for future tuning.
    lines.append(".global CDECL(fp_sqr)")
    lines.append("CDECL(fp_sqr):")
    lines.append("// void fp_sqr(fp_t *r, const fp_t *a); == fp_mul(r, a, a)")
    lines.append("mov    rdx, rsi")
    lines.append("jmp    CDECL(fp_mul)")
    lines.append("")

    sqr_plan = choose_sqr_registers(n)
    if sqr_plan is not None:
        product_regs, sqr_hi, sqr_lo, sqr_to_save = sqr_plan
        lines.append(".global CDECL(fp_sqr_dedicated)")
        lines.append("CDECL(fp_sqr_dedicated):")
        lines.append("// void fp_sqr_dedicated(fp_t *r, const fp_t *a); not currently used")
        lines.append("// (see emit_sqr_body); generated for future tuning.")
        lines.append("// SysV: rdi = r, rsi = a (no third pointer, so rcx is free to use).")
        sqr_frame = ((n * 8 + 15) // 16) * 16
        for r in sqr_to_save:
            lines.append(f"push   {r}")
        if sqr_frame:
            lines.append(f"sub    rsp, {sqr_frame}  // final reduction's masked-p staging")
        lines.append("")
        lines.extend(emit_sqr_body(P, product_regs, sqr_hi, sqr_lo))
        result_regs = product_regs[n:]
        lines.append("// canonicalize: result is < 2p; subtract p once if needed")
        lines.extend(emit_final_reduce(result_regs, n, sqr_hi, sqr_lo, "rsp"))
        lines.append("")
        for i in range(n):
            lines.append(f"mov    [rdi + {8 * i}], {result_regs[i]}" if i else f"mov    [rdi], {result_regs[i]}")
        lines.append("")
        if sqr_frame:
            lines.append(f"add    rsp, {sqr_frame}")
        for r in reversed(sqr_to_save):
            lines.append(f"pop    {r}")
        lines.append("ret")
        lines.append("")

    if choose_sop_registers(n) is not None:
        lines.extend(emit_sop_routine(P, "fp_sum_of_products", difference=False))
        lines.extend(emit_sop_routine(P, "fp_difference_of_products", difference=True))

    hadamard_lines = emit_hadamard_routine(P)
    if hadamard_lines is not None:
        lines.extend(hadamard_lines)

    lines.append("#if defined(__linux__) && defined(__ELF__)")
    lines.append('.section .note.GNU-stack,"",%progbits')
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def parse_prime(text: str) -> int:
    text = text.strip()
    try:
        return int(eval(text, {"__builtins__": {}}, {}))
    except Exception:
        return int(text, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("prime", nargs="?", help="prime as decimal, 0x-prefixed, or a 'c * 2**t - 1' expression")
    ap.add_argument("--prime", dest="prime_opt", help="same as the positional form")
    ap.add_argument("-o", "--output", default="fp_asm.S")
    args = ap.parse_args()

    text = args.prime_opt or args.prime
    if not text:
        ap.error("provide a prime")
    p = parse_prime(text)

    out = generate(p)
    Path(args.output).write_text(out)
    P = AsmParams(p)
    print("wrote %s: p = %d bits, %d limbs" % (args.output, P.bits, P.n))


if __name__ == "__main__":
    main()
