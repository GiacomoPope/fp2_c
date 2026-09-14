p248_5_VALUE  := "5 * 2**248 - 1"
p_coral_VALUE := "51 * 2**2026 - 1"
# p308_633_VALUE := 0x278fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
# p628_317_VALUE := 0x13cfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff

# PRIMES    := p248_5 p308_633 p628_317
PRIMES        := p248_5 p_coral

CC        := gcc
CFLAGS    := -Wall -Wextra -std=c99 -O3
PYTHON    := python3

BUILD_DIR := build
GEN_SCRIPT:= generator/gen_fp.py

# Benchmark binary for the static Scott benchmark
# TODO: generalise this to allow building multiple scott benchmarks
BENCH_SCOTT_248_BIN := $(BUILD_DIR)/bench_scott_p248
BENCH_SCOTT_CORAL_BIN := $(BUILD_DIR)/bench_scott_coral



# Lists of generated test and benchmark binaries
TEST_BINS  := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp) \
#               $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp2)
BENCH_BINS := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp) \
#               $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp2)

.PHONY: all build-tests tests bench clean

all: tests

build-tests: $(TEST_BINS)

tests: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "=== Running $$test ==="; \
		./$$test || exit 1; \
	done

bench: $(BENCH_BINS) $(BENCH_SCOTT_248_BIN) $(BENCH_SCOTT_CORAL_BIN)
	@for bench in $(BENCH_BINS); do \
		echo "=== Running $$bench ==="; \
		./$$bench || exit 1; \
	done
	@echo "=== Running $(BENCH_SCOTT_248_BIN) ==="
	@./$(BENCH_SCOTT_248_BIN)
	@echo "=== Running $(BENCH_SCOTT_CORAL_BIN) ==="
	@./$(BENCH_SCOTT_CORAL_BIN)


$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BENCH_SCOTT_248_BIN): src/scott/p_248/fp_scott.c src/fp_scott_bench.c include/fp_scott_bench.h bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -Isrc/scott/p_248 src/fp_scott_bench.c bench/bench_scott.c -o $@

$(BENCH_SCOTT_CORAL_BIN): src/scott/p_coral/fp_scott.c src/fp_scott_bench.c include/fp_scott_bench.h bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -Isrc/scott/p_coral src/fp_scott_bench.c bench/bench_scott.c -o $@

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

$(BUILD_DIR)/%/bench_fp: bench/bench_fp.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h bench/bench_utils.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c -o $@

# $(BUILD_DIR)/%/test_fp2: tests/test_fp2.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h include/fp2.h | $(BUILD_DIR)
# 	@mkdir -p $(BUILD_DIR)/$*
# 	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/fp2.c -o $@

# $(BUILD_DIR)/%/bench_fp2: bench/bench_fp2.c src/generated/%/fp.c include/generated/%/fp_defs.h include/fp.h include/fp2.h bench/bench_utils.h | $(BUILD_DIR)
# 	@mkdir -p $(BUILD_DIR)/$*
# 	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c src/fp2.c -o $@

clean:
	rm -rf $(BUILD_DIR) include/generated src/generated
