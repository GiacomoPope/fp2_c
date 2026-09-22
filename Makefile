p308_633_VALUE := "633 * 2**308 - 1"
p474_593_VALUE := "593 * 2**474 - 1"
p628_317_VALUE := "317 * 2**628 - 1"

PRIMES           := p308_633 p474_593 p628_317
BROADWELL_PRIMES := p308_633 p474_593 p628_317
U32_PRIMES       := p308_633 p474_593 p628_317

p308_633_MODARITH_U64_DIR := u64/p_308_633
p474_593_MODARITH_U64_DIR := u64/p_474_593
p628_317_MODARITH_U64_DIR := u64/p_628_317

p308_633_MODARITH_U32_DIR := u32/p308_633
p474_593_MODARITH_U32_DIR := u32/p474_593
p628_317_MODARITH_U32_DIR := u32/p628_317

CC        := clang
CFLAGS    := -Wall -Wextra -std=c99 -O3 -march=native
PYTHON    := python3

BUILD_DIR := build
BUILD_DIR_BROADWELL := build_broadwell
BUILD_DIR_U32 := build_u32
GEN_SCRIPT:= generator/gen_fp.py
GEN_SCRIPT_U32 := generator/gen_fp_u32.py

# Lists of generated test and benchmark binaries
TEST_BINS  := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp) \
              $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp2)
BENCH_BINS := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp) \
              $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp2)

BENCH_MODARITH_BINS     := $(foreach p,$(PRIMES),$(BUILD_DIR)/bench_modarith_$(p)/bench_fp) \
                         $(foreach p,$(PRIMES),$(BUILD_DIR)/bench_modarith_$(p)/bench_fp2)
BENCH_MODARITH_U32_BINS := $(foreach p,$(U32_PRIMES),$(BUILD_DIR_U32)/bench_modarith_$(p)/bench_fp) \
                         $(foreach p,$(U32_PRIMES),$(BUILD_DIR_U32)/bench_modarith_$(p)/bench_fp2)

TEST_BROADWELL_BINS  := $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/test_fp) \
                         $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/test_fp2)
BENCH_BROADWELL_BINS := $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/bench_fp) \
                         $(foreach p,$(BROADWELL_PRIMES),$(BUILD_DIR_BROADWELL)/$(p)/bench_fp2)

TEST_U32_BINS  := $(foreach p,$(U32_PRIMES),$(BUILD_DIR_U32)/$(p)/test_fp) \
                   $(foreach p,$(U32_PRIMES),$(BUILD_DIR_U32)/$(p)/test_fp2)
BENCH_U32_BINS := $(foreach p,$(U32_PRIMES),$(BUILD_DIR_U32)/$(p)/bench_fp) \
                   $(foreach p,$(U32_PRIMES),$(BUILD_DIR_U32)/$(p)/bench_fp2)

.PHONY: all build-tests tests bench clean tests-broadwell bench-broadwell tests-u32 bench-u32

all: tests

build-tests: $(TEST_BINS)

tests: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "=== Running $$test ==="; \
		./$$test || exit 1; \
	done

bench: $(BENCH_BINS) $(BENCH_MODARITH_BINS)
	@for bench in $(BENCH_BINS) $(BENCH_MODARITH_BINS); do \
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

tests-u32: $(TEST_U32_BINS)
	@for test in $(TEST_U32_BINS); do \
		echo "=== Running $$test ==="; \
		./$$test || exit 1; \
	done

bench-u32: $(BENCH_U32_BINS) $(BENCH_MODARITH_U32_BINS)
	@for bench in $(BENCH_U32_BINS) $(BENCH_MODARITH_U32_BINS); do \
		echo "=== Running $$bench ==="; \
		./$$bench || exit 1; \
	done

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

define MODARITH_BENCH_RULE
$(BUILD_DIR)/bench_modarith_$(1)/bench_fp: src/modarith/$($(1)_MODARITH_U64_DIR)/fp_scott.c bench/bench_fp.c \
                                         include/modarith/fp.h include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c \
                                         include/theta_dim4.h src/theta_dim4.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/bench_modarith_$(1)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude/modarith -Isrc/modarith/$($(1)_MODARITH_U64_DIR) -Iinclude \
		bench/bench_fp.c src/two_two_isogeny_chain.c src/theta_dim4.c -o $$@

$(BUILD_DIR)/bench_modarith_$(1)/bench_fp2: src/modarith/$($(1)_MODARITH_U64_DIR)/fp_scott.c bench/bench_fp2.c \
                                          include/modarith/fp.h include/fp2.h src/fp2.c \
                                          include/four_isogeny_chain.h src/four_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/bench_modarith_$(1)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude/modarith -Isrc/modarith/$($(1)_MODARITH_U64_DIR) -Iinclude \
		bench/bench_fp2.c src/fp2.c src/four_isogeny_chain.c -o $$@
endef

$(foreach p,$(PRIMES),$(eval $(call MODARITH_BENCH_RULE,$(p))))

define MODARITH_U32_BENCH_RULE
$(BUILD_DIR_U32)/bench_modarith_$(1)/bench_fp: src/modarith/$($(1)_MODARITH_U32_DIR)/fp_scott.c bench/bench_fp.c \
                                             include/modarith/fp.h include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c \
                                             include/theta_dim4.h src/theta_dim4.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_U32)/bench_modarith_$(1)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude/modarith -Isrc/modarith/$($(1)_MODARITH_U32_DIR) -Iinclude \
		bench/bench_fp.c src/two_two_isogeny_chain.c src/theta_dim4.c -o $$@

$(BUILD_DIR_U32)/bench_modarith_$(1)/bench_fp2: src/modarith/$($(1)_MODARITH_U32_DIR)/fp_scott.c bench/bench_fp2.c \
                                              include/modarith/fp.h include/fp2.h src/fp2.c \
                                              include/four_isogeny_chain.h src/four_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_U32)/bench_modarith_$(1)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude/modarith -Isrc/modarith/$($(1)_MODARITH_U32_DIR) -Iinclude \
		bench/bench_fp2.c src/fp2.c src/four_isogeny_chain.c -o $$@
endef

$(foreach p,$(U32_PRIMES),$(eval $(call MODARITH_U32_BENCH_RULE,$(p))))

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
                         include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c \
                         include/theta_dim4.h src/theta_dim4.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/two_two_isogeny_chain.c src/theta_dim4.c -o $@

$(BUILD_DIR)/%/test_fp2: tests/test_fp2.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h include/fp2.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/fp2.c -o $@

$(BUILD_DIR)/%/bench_fp2: bench/bench_fp2.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h include/fp2.h \
                          include/four_isogeny_chain.h src/four_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/fp2.c src/four_isogeny_chain.c -o $@

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
                                    include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c \
                                    include/theta_dim4.h src/theta_dim4.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_BROADWELL)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_broadwell/$* -Iinclude $< \
		src/generated_broadwell/$*/fp.c src/generated_broadwell/$*/fp_asm.S src/two_two_isogeny_chain.c src/theta_dim4.c -o $@

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

.PRECIOUS: include/generated_u32/%/fp_defs.h src/generated_u32/%/fp.c

include/generated_u32/%/fp_defs.h src/generated_u32/%/fp.c: $(GEN_SCRIPT_U32)
	@mkdir -p include/generated_u32/$* src/generated_u32/$*
	$(PYTHON) $(GEN_SCRIPT_U32) $($*_VALUE) \
		--include-dir include/generated_u32/$* \
		--source-dir src/generated_u32/$*

$(BUILD_DIR_U32)/%/test_fp: tests/test_fp.c src/generated_u32/%/fp.c include/generated_u32/%/fp_defs.h include/fp.h include/util_32.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_U32)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_u32/$* -Iinclude $< src/generated_u32/$*/fp.c -o $@

$(BUILD_DIR_U32)/%/bench_fp: bench/bench_fp.c src/generated_u32/%/fp.c include/generated_u32/%/fp_defs.h include/fp.h include/util_32.h \
                             include/two_two_isogeny_chain.h src/two_two_isogeny_chain.c \
                             include/theta_dim4.h src/theta_dim4.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_U32)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_u32/$* -Iinclude $< src/generated_u32/$*/fp.c src/two_two_isogeny_chain.c src/theta_dim4.c -o $@

$(BUILD_DIR_U32)/%/test_fp2: tests/test_fp2.c src/generated_u32/%/fp.c include/generated_u32/%/fp_defs.h include/fp.h include/util_32.h include/fp2.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_U32)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_u32/$* -Iinclude $< src/generated_u32/$*/fp.c src/fp2.c -o $@

$(BUILD_DIR_U32)/%/bench_fp2: bench/bench_fp2.c src/generated_u32/%/fp.c include/generated_u32/%/fp_defs.h include/fp.h include/util_32.h include/fp2.h \
                              include/four_isogeny_chain.h src/four_isogeny_chain.c bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR_U32)/$*
	$(CC) $(CFLAGS) -Iinclude/generated_u32/$* -Iinclude $< src/generated_u32/$*/fp.c src/fp2.c src/four_isogeny_chain.c -o $@

clean:
	rm -rf $(BUILD_DIR) $(BUILD_DIR_BROADWELL) $(BUILD_DIR_U32) include/generated src/generated \
		include/generated_broadwell src/generated_broadwell include/generated_u32 src/generated_u32
