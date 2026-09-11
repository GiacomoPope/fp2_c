p245_5_VALUE   := 0x4ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
p308_633_VALUE := 0x278fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
p628_317_VALUE := 0x13cfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff

PRIMES    := p245_5 p308_633 p628_317

CC        := gcc
CFLAGS    := -Wall -Wextra -O3 -std=c99
PYTHON    := python3

BUILD_DIR := build
GEN_SCRIPT:= generator/generate.py

# Benchmark binary for the static Scott benchmark
# TODO: generalise this to allow building multiple scott benchmarks
BENCH_SCOTT_BIN := $(BUILD_DIR)/bench_scott

# Lists of generated test and benchmark binaries
TEST_BINS  := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/test_fp)
BENCH_BINS := $(foreach p,$(PRIMES),$(BUILD_DIR)/$(p)/bench_fp)

.PHONY: all build-tests tests bench clean

all: tests

build-tests: $(TEST_BINS)

tests: $(TEST_BINS)
	@for test in $(TEST_BINS); do \
		echo "=== Running $$test ==="; \
		./$$test || exit 1; \
	done

bench: $(BENCH_BINS) $(BENCH_SCOTT_BIN)
	@for bench in $(BENCH_BINS); do \
		echo "=== Running $$bench ==="; \
		./$$bench || exit 1; \
	done
	@echo "=== Running $(BENCH_SCOTT_BIN) ==="
	@./$(BENCH_SCOTT_BIN)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BENCH_SCOTT_BIN): src/fp_scott.c src/fp_scott_bench.c include/fp_scott_bench.h bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude src/fp_scott_bench.c bench/bench_scott.c -o $@

# Outputs directly to root include/generated/<prime> and src/generated/<prime>
.PRECIOUS: include/generated/%/fp.h src/generated/%/fp.c

include/generated/%/fp.h src/generated/%/fp.c: $(GEN_SCRIPT)
	@mkdir -p include/generated/$* src/generated/$*
	$(PYTHON) $(GEN_SCRIPT) $($*_VALUE) \
		--include-dir include/generated/$* \
		--source-dir src/generated/$*

$(BUILD_DIR)/%/test_fp: tests/test_fp.c src/generated/%/fp.c include/generated/%/fp.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c -o $@

$(BUILD_DIR)/%/bench_fp: bench/bench_fp.c src/generated/%/fp.c include/generated/%/fp.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$*
	$(CC) $(CFLAGS) -Iinclude/generated/$* -Iinclude $< src/generated/$*/fp.c -o $@

clean:
	rm -rf $(BUILD_DIR) include/generated src/generated
