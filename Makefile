p308_633_VALUE := "633 * 2**308 - 1"
p474_593_VALUE := "593 * 2**474 - 1"
p628_317_VALUE := "317 * 2**628 - 1"
# p_coral_VALUE := "51 * 2**2026 - 1"

PRIMES        := p308_633 p474_593 p628_317 # p_coral

# Scott's own benchmark keeps its generated code under src/scott/p_XXX,
# named independently of our p308_633-style prime identifiers.
p308_633_SCOTT_DIR := p_308
p474_593_SCOTT_DIR := p_474
p628_317_SCOTT_DIR := p_628
# p_coral_SCOTT_DIR  := p_coral

CC        := clang
CFLAGS    := -Wall -Wextra -std=c99 -O3 -march=native
PYTHON    := python3

BUILD_DIR := build
GEN_SCRIPT:= generator/gen_fp.py

# Lists of generated test and benchmark binaries
TEST_BINS  := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp) \
              $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp2)
BENCH_BINS := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp) \
              $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp2)

# Mike Scott's reference benchmark, and our own isogeny chains recompiled
# against his field arithmetic via include/scott/fp.h. One binary of each
# per prime.
BENCH_SCOTT_BINS        := $(foreach p,$(PRIMES),$(BUILD_DIR)/bench_scott_$(p))
BENCH_SCOTT_CHAINS_BINS := $(foreach p,$(PRIMES),$(BUILD_DIR)/bench_scott_chains_$(p))

.PHONY: all build-tests tests bench clean

all: tests

build-tests: $(TEST_BINS)

tests: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "=== Running $$test ==="; \
		./$$test || exit 1; \
	done

bench: $(BENCH_BINS) $(BENCH_SCOTT_BINS) $(BENCH_SCOTT_CHAINS_BINS)
	@for bench in $(BENCH_BINS) $(BENCH_SCOTT_BINS) $(BENCH_SCOTT_CHAINS_BINS); do \
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

$(foreach p,$(PRIMES),$(eval $(call SCOTT_BENCH_RULE,$(p))))
$(foreach p,$(PRIMES),$(eval $(call SCOTT_CHAINS_BENCH_RULE,$(p))))

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

clean:
	rm -rf $(BUILD_DIR) include/generated src/generated
