p308_633_VALUE := "633 * 2**308 - 1"
p474_593_VALUE := "593 * 2**474 - 1"
p628_317_VALUE := "317 * 2**628 - 1"

PRIMES           := p308_633 p474_593 p628_317
BROADWELL_PRIMES := p308_633 p474_593 p628_317

# Scott's own benchmark keeps its generated code under src/scott/p_XXX,
# named independently of our p308_633-style prime identifiers.
p308_633_SCOTT_DIR := p_308
p474_593_SCOTT_DIR := p_474
p628_317_SCOTT_DIR := p_628

CC        := clang
CFLAGS    := -Wall -Wextra -std=c99 -O3 -march=native
PYTHON    := python3

BUILD_DIR := build
BUILD_DIR_BROADWELL := build_broadwell
GEN_SCRIPT:= generator/gen_fp.py

# Lists of generated test and benchmark binaries
TEST_BINS  := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp) \
              $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp2)
BENCH_BINS := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp) \
              $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp2) \
              $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_hadamard)

# Mike Scott's reference benchmark, and our own isogeny chains recompiled
# against his field arithmetic via include/scott/fp.h. One binary of each
# per prime.
BENCH_SCOTT_BINS        := $(foreach p,$(PRIMES),$(BUILD_DIR)/bench_scott_$(p))
BENCH_SCOTT_CHAINS_BINS := $(foreach p,$(PRIMES),$(BUILD_DIR)/bench_scott_chains_$(p))
BENCH_SCOTT_FP2_BINS    := $(foreach p,$(PRIMES),$(BUILD_DIR)/bench_scott_fp2_$(p))

TEST_BROADWELL_BINS  := $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/test_fp) \
                         $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/test_fp2)
BENCH_BROADWELL_BINS := $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/bench_fp) \
                         $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/bench_fp2) \
                         $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/bench_hadamard)

.PHONY: all build-tests tests bench clean tests-broadwell bench-broadwell

all: tests

build-tests: $(TEST_BINS)

tests: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "=== Running $$test ==="; \
		./$$test || exit 1; \
	done

bench: $(BENCH_BINS) $(BENCH_SCOTT_BINS) $(BENCH_SCOTT_CHAINS_BINS) $(BENCH_SCOTT_FP2_BINS)
	@for bench in $(BENCH_BINS) $(BENCH_SCOTT_BINS) $(BENCH_SCOTT_CHAINS_BINS) $(BENCH_SCOTT_FP2_BINS); do \
		echo "=== Running $$bench ==="; \
		./$$bench || exit 1; \
	done

tests-broadwell: $(TEST_BROADWELL_BINS)
	@for test in $(TEST_BROADWELL_BINS); do \
		echo "=== Running $$test ==="; \
		./$$test || exit 1; \
	done

bench-broadwell: $(BENCH_BROADWELL_BINS)
	@for bench in $(BENCH_BROADWELL_BINS); do \
		echo "=== Running $$bench ==="; \
		./$$bench || exit 1; \
	done


$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# One bench_scott_<prime> and bench_scott_chains_<prime> rule per entry in
# PRIMES, generated from these templates instead of hand-duplicated.
define SCOTT_BENCH_RULE
$(BUILD_DIR)/bench_scott_$(1): src/scott/$($(1)_SCOTT_DIR)/fp_scott.c bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -Isrc/scott/$($(1)_SCOTT_DIR) bench/bench_scott.c -o $$@
endef

define SCOTT_CHAINS_BENCH_RULE
$(BUILD_DIR)/bench_scott_chains_$(1): src/scott/$($(1)_SCOTT_DIR)/fp_scott.c bench/bench_scott_chains.c \
                                       include/scott/fp.h include/fp2.h src/fp2.c \
                                       include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c \
                                       include/four_isogeny_chain.h src/four_isogeny_chain.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude/scott -Isrc/scott/$($(1)_SCOTT_DIR) -Iinclude \
		bench/bench_scott_chains.c src/fp2.c src/two_two_isogeny_chain.c src/four_isogeny_chain.c -o $$@
endef

define SCOTT_FP2_BENCH_RULE
$(BUILD_DIR)/bench_scott_fp2_$(1): src/scott/$($(1)_SCOTT_DIR)/fp_scott.c bench/bench_fp2.c \
                                    include/scott/fp.h include/fp2.h src/fp2.c \
                                    include/four_isogeny_chain.h src/four_isogeny_chain.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude/scott -Isrc/scott/$($(1)_SCOTT_DIR) -Iinclude \
		bench/bench_fp2.c src/fp2.c src/four_isogeny_chain.c -o $$@
endef

$(foreach p,$(PRIMES),$(eval $(call SCOTT_BENCH_RULE,$(p))))
$(foreach p,$(PRIMES),$(eval $(call SCOTT_CHAINS_BENCH_RULE,$(p))))
$(foreach p,$(PRIMES),$(eval $(call SCOTT_FP2_BENCH_RULE,$(p))))

# Outputs directly to root include/generated/<prime> and src/generated/<prime>
.PRECIOUS: include/generated/%/fp_defs.h src/generated/%/fp.c

include/generated/%/fp_defs.h src/generated/%/fp.c: $(GEN_SCRIPT)
	@mkdir -p include/generated/$* src/generated/$*
	$(PYTHON) $(GEN_SCRIPT) $($*_VALUE) \
		--include-dir include/generated/$* \
		--source-dir src/generated/$*

$(BUILD_DIR)/%/test_fp: tests/test_fp.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c -o $@

$(BUILD_DIR)/%/bench_fp: bench/bench_fp.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h \
                         include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/two_two_isogeny_chain.c -o $@

$(BUILD_DIR)/%/test_fp2: tests/test_fp2.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h include/fp2.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/fp2.c -o $@

$(BUILD_DIR)/%/bench_fp2: bench/bench_fp2.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h include/fp2.h \
                          include/four_isogeny_chain.h src/four_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/fp2.c src/four_isogeny_chain.c -o $@

$(BUILD_DIR)/%/bench_hadamard: bench/bench_hadamard.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h \
                                include/theta_dim4.h src/theta_dim4.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/theta_dim4.c -o $@

.PRECIOUS: include/generated_broadwell/%/fp_defs.h src/generated_broadwell/%/fp.c src/generated_broadwell/%/fp_asm.S

include/generated_broadwell/%/fp_defs.h src/generated_broadwell/%/fp.c src/generated_broadwell/%/fp_asm.S: \
                generator/gen_fp_broadwell.py generator/gen_fp.py generator/gen_fp_asm_broadwell.py
	@mkdir -p include/generated_broadwell/$* src/generated_broadwell/$*
	$(PYTHON) generator/gen_fp_broadwell.py $($*_VALUE) \
		--include-dir include/generated_broadwell/$* \
		--source-dir src/generated_broadwell/$*

$(BUILD_DIR_BROADWELL)/%/test_fp: tests/test_fp.c src/generated_broadwell/%/fp.c src/generated_broadwell/%/fp_asm.S \
                                   include/generated_broadwell/%/fp_defs.h include/fp.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_BROADWELL)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_broadwell/$* -Iinclude $< \
		src/generated_broadwell/$*/fp.c src/generated_broadwell/$*/fp_asm.S -o $@

$(BUILD_DIR_BROADWELL)/%/bench_fp: bench/bench_fp.c src/generated_broadwell/%/fp.c src/generated_broadwell/%/fp_asm.S \
                                    include/generated_broadwell/%/fp_defs.h include/fp.h \
                                    include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_BROADWELL)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_broadwell/$* -Iinclude $< \
		src/generated_broadwell/$*/fp.c src/generated_broadwell/$*/fp_asm.S src/two_two_isogeny_chain.c -o $@

$(BUILD_DIR_BROADWELL)/%/test_fp2: tests/test_fp2.c src/generated_broadwell/%/fp.c src/generated_broadwell/%/fp_asm.S \
                                    include/generated_broadwell/%/fp_defs.h include/fp.h include/fp2.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_BROADWELL)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_broadwell/$* -Iinclude $< \
		src/generated_broadwell/$*/fp.c src/generated_broadwell/$*/fp_asm.S src/fp2.c -o $@

$(BUILD_DIR_BROADWELL)/%/bench_fp2: bench/bench_fp2.c src/generated_broadwell/%/fp.c src/generated_broadwell/%/fp_asm.S \
                                     include/generated_broadwell/%/fp_defs.h include/fp.h include/fp2.h \
                                     include/four_isogeny_chain.h src/four_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_BROADWELL)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_broadwell/$* -Iinclude $< \
		src/generated_broadwell/$*/fp.c src/generated_broadwell/$*/fp_asm.S src/fp2.c src/four_isogeny_chain.c -o $@

$(BUILD_DIR_BROADWELL)/%/bench_hadamard: bench/bench_hadamard.c src/generated_broadwell/%/fp.c src/generated_broadwell/%/fp_asm.S \
                                          include/generated_broadwell/%/fp_defs.h include/fp.h \
                                          include/theta_dim4.h src/theta_dim4.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_BROADWELL)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_broadwell/$* -Iinclude $< \
		src/generated_broadwell/$*/fp.c src/generated_broadwell/$*/fp_asm.S src/theta_dim4.c -o $@

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR_BROADWELL) include/generated src/generated include/generated_broadwell src/generated_broadwell
