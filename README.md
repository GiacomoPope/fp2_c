This is a small repo which is a work in progress adaptation of the macro from https://github.com/GiacomoPope/fp2 so I can have familiar arithmetic for C projects.

The goal is to have an easy way to generate finite field arithmetic for isogeny based cryptography.

## Benchmarks

Cycle counts measured using 2.6 GHz Intel Core i7 CPU with Turbo Boost disabled.

- **Portable C**: `generator/gen_fp.py`'s output, plain `-O3 -march=native`, although there's no hand-written assembly, we do attempt to use best case intrinsics for word level operations.
- **ASM (Broadwell)**: `generator/gen_fp_broadwell.py`'s output x86-64 assembly (MULX/ADCX/ADOX) for the functions `fp_mul`, `fp_sqr`, `fp_sum_of_products` and `fp_difference_of_products` using the portable C for everything else.
- **Scott**: Mike Scott's [`modarith`](https://github.com/mcarrickscott/modarith) reference field-arithmetic generator, included as an external comparison point created using `python3 monty.py "prime"`. 

NOTE: to compare `GF(p^2)` arithmetic between this generator and Scott's code, we write a simple wrapper which aligns the API of the base field arithmetic. This is what (for example) is done in SQIsign as well where the Scott code is used for reference implementations.

### p308_633 (`633 * 2^308 - 1`, 5 limbs)

| Operation | Portable C | ASM (Broadwell) | Scott |
|---|---:|---:|---:|
| `fp_add` | 14 | 14 | 19 |
| `fp_mul` | 76 | 53 | 68 |
| `fp_sqr` | 50 | 54 | 64 |
| `fp_inv` | 11,088 | 11,098 | 22,018 |
| `fp_sqrt` | 19,829 | 23,050 | 21,970 |
| `dim_four_hadamard` | 769 | 769 | — |
| `two_two_isogeny_chain(len=128)` | 2,068,191 | 1,790,044 | 2,769,887 |


| Operation | Portable C | ASM (Broadwell) | Scott |
|---|---:|---:|---:|
| `fp2_add` | 25 | 25 | 42 |
| `fp2_mul` | 284 | 201 | 455 |
| `fp2_sqr` | 193 | 146 | 256 |
| `fp2_inv` | 11,390 | 11,375 | 22,609 |
| `fp2_sqrt` | 46,880 | 52,759 | 45,286 |
| `four_isogeny_chain(len=128)` | 1,151,251 | 846,318 | 1,729,871 |

### p474_593 (`593 * 2^474 - 1`, 8 limbs)

| Operation | Portable C | ASM (Broadwell) | Scott |
|---|---:|---:|---:|
| `fp_add` | 19 | 19 | 25 |
| `fp_mul` | 186 | 117 | 152 |
| `fp_sqr` | 131 | 117 | 98 |
| `fp_inv` | 21,114 | 21,125 | 51,546 |
| `fp_sqrt` | 70,352 | 62,008 | 52,191 |
| `dim_four_hadamard` | 1,238 | 1,238 | — |
| `two_two_isogeny_chain(len=128)` | 4,597,898 | 3,466,256 | 4,165,981 |

| Operation | Portable C | ASM (Broadwell) | Scott |
|---|---:|---:|---:|
| `fp2_add` | 36 | 36 | 56 |
| `fp2_mul` | 696 | 692 | 689 |
| `fp2_sqr` | 425 | 283 | 399 |
| `fp2_inv` | 21,894 | 21,728 | 52,325 |
| `fp2_sqrt` | 165,001 | 139,967 | 104,796 |
| `four_isogeny_chain(len=128)` | 2,610,164 | 2,437,470 | 2,616,559 |

### p628_317 (`317 * 2^628 - 1`, 10 limbs)

| Operation | Portable C | ASM (Broadwell) | Scott |
|---|---:|---:|---:|
| `fp_add` | 23 | 23 | 34 |
| `fp_mul` | 283 | 176 | 281 |
| `fp_sqr` | 190 | 178 | 159 |
| `fp_inv` | 31,637 | 31,486 | 112,288 |
| `fp_sqrt` | 130,388 | 119,670 | 111,002 |
| `dim_four_hadamard` | 1,558 | 1,558 | — |
| `two_two_isogeny_chain(len=128)` | 6,937,197 | 5,294,362 | 7,093,671 |

| Operation | Portable C | ASM (Broadwell) | Scott |
|---|---:|---:|---:|
| `fp2_add` | 45 | 45 | 99 |
| `fp2_mul` | 1,083 | 1,075 | 1,228 |
| `fp2_sqr` | 637 | 418 | 708 |
| `fp2_inv` | 32,697 | 32,355 | 113,222 |
| `fp2_sqrt` | 309,683 | 268,826 | 226,584 |
| `four_isogeny_chain(len=128)` | 4,044,612 | 3,736,953 | 4,818,339 |
