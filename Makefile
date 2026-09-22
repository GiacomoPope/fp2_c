p308_633_VALUE := "633 * 2**308 - 1"
p474_593_VALUE := "593 * 2**474 - 1"
p628_317_VALUE := "317 * 2**628 - 1"
p_coral_VALUE := "51 * 2**2026 - 1"

PRIMES        := p308_633 p474_593 p628_317 p_coral

CC        := clang
CFLAGS    := -Wall -Wextra -std=c99 -O3 -march=native
PYTHON    := python3

BUILD_DIR := build
GEN_SCRIPT:= generator/gen_fp.py

# Benchmark binary for the static Scott benchmark
# TODO: generalise this to allow building multiple scott benchmarks
BENCH_SCOTT_308_BIN := $(BUILD_DIR)/bench_scott_p308
BENCH_SCOTT_474_BIN := $(BUILD_DIR)/bench_scott_p474
BENCH_SCOTT_628_BIN := $(BUILD_DIR)/bench_scott_p628
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

bench: $(BENCH_BINS) $(BENCH_SCOTT_308_BIN)  $(BENCH_SCOTT_474_BIN)  $(BENCH_SCOTT_628_BIN) $(BENCH_SCOTT_CORAL_BIN)
	@for bench in $(BENCH_BINS); do \
		echo "=== Running $$bench ==="; \
		./$$bench || exit 1; \
	done
	@echo "=== Running $(BENCH_SCOTT_308_BIN) ==="
	@./$(BENCH_SCOTT_308_BIN)
	@echo "=== Running $(BENCH_SCOTT_474_BIN) ==="
	@./$(BENCH_SCOTT_474_BIN)
	@echo "=== Running $(BENCH_SCOTT_628_BIN) ==="
	@./$(BENCH_SCOTT_628_BIN)
	@echo "=== Running $(BENCH_SCOTT_CORAL_BIN) ==="
	@./$(BENCH_SCOTT_CORAL_BIN)


$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BENCH_SCOTT_308_BIN): src/scott/p_308/fp_scott.c bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -Isrc/scott/p_308 bench/bench_scott.c -o $@

$(BENCH_SCOTT_474_BIN): src/scott/p_474/fp_scott.c bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -Isrc/scott/p_474 bench/bench_scott.c -o $@

$(BENCH_SCOTT_628_BIN): src/scott/p_628/fp_scott.c bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -Isrc/scott/p_628 bench/bench_scott.c -o $@

$(BENCH_SCOTT_CORAL_BIN): src/scott/p_coral/fp_scott.c bench/bench_scott.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -Isrc/scott/p_coral bench/bench_scott.c -o $@

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
