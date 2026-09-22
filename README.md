This is a small repo which is a work in progress adaptation of the macro from https://github.com/GiacomoPope/fp2 so I can have familiar arithmetic for C projects.

The goal is to have an easy way to generate finite field arithmetic for isogeny based cryptography.

## Summary

I'm not sure there's enough of a reason to use this code over the reference generation `modarith` at the moment. The `x86` assembly is faster, but that's not an interesting statement. The saturated arithmetic has faster additions (this helps for high dimensional isogeny chains with expensive Hadamards) but the `fp_mul` and `fp_sqr` performance is not significantly better and in some cases is worse. Legendre signs and inversions are much faster in this code, but this is not because of the internal representation, but instead because we use Thomas Pornin's Binary GCD instead of exponentiation.

One thing that is faster (generally, but not always) is the `fp2` arithmetic, mainly as we have generated functions for the sums and differences of products following Patrick Longa's nice work: [Efficient Algorithms for Large Prime Characteristic Fields and Their Application to Bilinear Pairings](https://eprint.iacr.org/2022/367.pdf)

I'll keep tweaking this and see if I can get something more interesting.

## AI Disclosure 

To be explicit: there's python code in this repo which generates `x86` ASM for these primes, which I did not write. I asked some LLMs to look at other ASM generators i found online and asked it to align these ideas with the internal representation used in the portable C. I'm not super happy about this and maybe will revisit this with my own code, but I have little motivation to hand write assembly and the fact a robot can do it for me while letting me focus on writing more isogeny code (or even pure C fp arithmetic code) is fine by me.

## Benchmarks

Cycle counts measured on an Intel Core i7-9750H @ 2.6 GHz, Turbo Boost disabled.

- **Portable C**: `generator/gen_fp.py` / `generator/gen_fp_u32.py`, Saturated representation written in pure C (no hand written ASM). However, intrinsics are opportunistically used for word-level operations as in the `fp2_rs` library.
- **ASM (Broadwell)**: `generator/gen_fp_broadwell.py`'s x86-64 assembly (MULX/ADCX/ADOX) for `fp_mul`/`fp_sqr`/`fp_sum_of_products`/`fp_difference_of_products` based off (loosely) the [SQIsign fp generator](https://github.com/SQISign/the-sqisign/tree/main/scripts/gen_fp) by asking a LLM for help. I don't like using LLMs, but I also don't like writing ASM. If there's a human interested in improving this library, I would love to have Human Code replace this section.
- **Modarith**: Mike Scott's [`modarith`](https://github.com/mcarrickscott/modarith) reference generator, for both 32 and 64 bit limbs, as an external reference. Note that `modarith` uses unsaturated representations.

NOTE: A prime named `pe_c` is of the form $$p = c \cdot 2^{e} - 1$$.

NOTE: the generated C code here has much faster times than `modarith` for inversion as we use [Thomas Pornin's Binary GCD](https://eprint.iacr.org/2020/972) built following the [`crrl`](https://github.com/pornin/crrl/) library.

### p308_633 `[u64; 5]` / `[u32; 10]`

| `fp` | C `u64` | ASM `u64` | Modarith `u64` | C `u32` | Modarith `u32` |
|---|---:|---:|---:|---:|---:|
| `add` | 14 | 14 | 19 | 23 | 37 |
| `mul` | 87 | 64 | 89 | 238 | 205 |
| `sqr` | 62 | 63 | 68 | 178 | 124 |
| `inv` | 11,106 | 11,115 | 22,216 | 26,344 | 45,854 |
| `sqrt` | 19,845 | 24,251 | 22,384 | 62,321 | 45,577 |
| `dim_four_hadamard` | 769 | 769 | 1,289 | 1,554 | 3,005 |
| `(2^128,2^128)-isogeny` | 2,071,742 | 1,794,126 | 2,716,075 | 6,079,442 | 6,178,526 |

| `fp2` | C `u64` | ASM `u64` | Modarith `u64` | C `u32` | Modarith `u32` |
|---|---:|---:|---:|---:|---:|
| `add` | 25 | 25 | 43 | 45 | 119 |
| `mul` | 287 | 201 | 448 | 949 | 974 |
| `sqr` | 193 | 147 | 255 | 530 | 626 |
| `inv` | 11,440 | 11,361 | 22,601 | 27,092 | 46,777 |
| `sqrt` | 46,837 | 52,601 | 45,276 | 145,814 | 93,768 |
| `(4^64)-isogeny` | 1,154,449 | 854,342 | 1,732,922 | 3,527,327 | 3,925,068 |

### p474_593 `[u64; 8]` / `[u32; 16]`

| `fp` | C `u64` | ASM `u64` | Modarith `u64` | C `u32` | Modarith `u32` |
|---|---:|---:|---:|---:|---:|
| `add` | 19 | 19 | 25 | 57 | 59 |
| `mul` | 186 | 117 | 152 | 652 | 412 |
| `sqr` | 131 | 117 | 109 | 697 | 218 |
| `inv` | 21,270 | 21,149 | 51,747 | 59,340 | 120,782 |
| `sqrt` | 70,237 | 61,946 | 52,155 | 355,923 | 120,193 |
| `dim_four_hadamard` | 1,238 | 1,238 | 1,700 | 3,462 | 4,311 |
| `(2^128,2^128)-isogeny` | 4,621,805 | 3,455,877 | 4,169,812 | 19,173,647 | 11,263,392 |

| `fp2` | C `u64` | ASM `u64` | Modarith `u64` | C `u32` | Modarith `u32` |
|---|---:|---:|---:|---:|---:|
| `add` | 36 | 36 | 56 | 117 | 182 |
| `mul` | 701 | 695 | 688 | 2,946 | 1,856 |
| `sqr` | 426 | 283 | 391 | 1,462 | 1,125 |
| `inv` | 21,894 | 21,719 | 52,325 | 62,001 | 121,710 |
| `sqrt` | 164,942 | 139,762 | 104,792 | 791,497 | 244,081 |
| `(4^64)-isogeny` | 2,646,316 | 2,446,602 | 2,618,724 | 10,475,914 | 7,248,918 |

NOTE: this prime performs the worst, as the `modarith` library can use an unsaturated representation without gaining a limb for the `u64` representation and for the `u32` representation the Montgomery reduction is less efficient due to where the top words end up...

---

### p628_317 `[u64; 10]` / `[u32; 20]`

| `fp` | C `u64` | ASM `u64` | Modarith `u64` | C `u32` | Modarith `u32` |
|---|---:|---:|---:|---:|---:|
| `add` | 23 | 23 | 34 | 73 | 86 |
| `mul` | 286 | 177 | 281 | 925 | 751 |
| `sqr` | 191 | 177 | 160 | 626 | 385 |
| `inv` | 31,436 | 31,471 | 111,118 | 96,158 | 271,321 |
| `sqrt` | 130,832 | 120,342 | 111,785 | 417,190 | 275,078 |
| `dim_four_hadamard` | 1,557 | 1,557 | 2,550 | 4,582 | 6,396 |
| `(2^128,2^128)-isogeny` | 6,946,029 | 5,321,889 | 7,102,776 | 22,497,448 | 19,875,051 |

| `fp2` | C `u64` | ASM `u64` | Modarith `u64` | C `u32` | Modarith `u32` |
|---|---:|---:|---:|---:|---:|
| `add` | 45 | 45 | 99 | 149 | 246 |
| `mul` | 1,095 | 1,083 | 1,228 | 3,860 | 3,252 |
| `sqr` | 642 | 417 | 708 | 2,047 | 1,901 |
| `inv` | 32,710 | 32,332 | 112,253 | 98,957 | 273,846 |
| `sqrt` | 309,699 | 268,338 | 226,535 | 983,588 | 549,409 |
| `(4^64)-isogeny` | 4,059,571 | 3,745,854 | 4,813,815 | 13,967,302 | 12,435,743 |

---

