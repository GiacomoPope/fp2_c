#!/usr/bin/env python3
"""
Generator of an fp.c/fp.h/fp_defs.h triple identical to
generator/gen_fp.py's output, except that fp_mul, fp_sqr and (limb count
permitting) fp_sum_of_products/fp_difference_of_products are left
undefined in the C source and implemented by hand-tuned Broadwell
assembly instead (see gen_fp_asm_broadwell.py), emitted as fp_asm.S.

fp_sqr's symbol always exists in the .S (currently a tail-call alias to
fp_mul; see gen_fp_asm_broadwell.py), so the C side never needs a
fallback for it. Sum/difference-of-products has no such cheap alias, so
past its register ceiling the .S omits those symbols and this generator
falls back to gen_fp.py's own fused portable-C implementation.

Every other function (add, sub, inversion, sqrt, encode/decode, ...) is
exactly gen_fp.py's portable C, already covered by its property tests.

Usage:
    python gen_fp_broadwell.py "633 * 2**308 - 1" \\
        --include-dir include/generated_broadwell/p308_633 \\
        --source-dir src/generated_broadwell/p308_633
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gen_fp import FieldGenerator, parse_int
import gen_fp_asm_broadwell


class BroadwellFieldGenerator(FieldGenerator):
    def generate_mul(self) -> str:
        return "// fp_mul: implemented in fp_asm.S (Broadwell MULX/ADCX/ADOX)\n"

    def generate_sqr(self) -> str:
        return "// fp_sqr: implemented in fp_asm.S (Broadwell MULX/ADCX/ADOX)\n"

    def generate_sum_of_products(self) -> str:
        if gen_fp_asm_broadwell.choose_sop_registers(self.n) is None:
            return super().generate_sum_of_products()
        return "// fp_sum_of_products: implemented in fp_asm.S (Broadwell MULX/ADCX/ADOX)\n"

    def generate_difference_of_products(self) -> str:
        if gen_fp_asm_broadwell.choose_sop_registers(self.n) is None:
            return super().generate_difference_of_products()
        return "// fp_difference_of_products: implemented in fp_asm.S (Broadwell MULX/ADCX/ADOX)\n"

    # fp_hadamard: left as gen_fp.py's portable-C fused version. The asm
    # equivalent (gen_fp_asm_broadwell.emit_hadamard_routine) was
    # benchmarked and found consistently slower -- see its docstring --
    # so it's generated as fp_hadamard_dedicated but not wired up here.

    def generate(self, include_dir: Path, source_dir: Path):
        super().generate(include_dir, source_dir)
        asm = gen_fp_asm_broadwell.generate(self.p)
        (source_dir / "fp_asm.S").write_text(asm)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("prime", help="prime as decimal or 0x-prefixed integer, or a Python expression like '633 * 2**308 - 1'")
    ap.add_argument("--include-dir", required=True)
    ap.add_argument("--source-dir", required=True)
    args = ap.parse_args()

    BroadwellFieldGenerator(parse_int(args.prime)).generate(
        Path(args.include_dir), Path(args.source_dir)
    )


if __name__ == "__main__":
    main()
