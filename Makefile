CC ?= cc
CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude
LDFLAGS ?=
GCOV ?= gcov
BUILD := build
BIN := bin/wise-combine-test
API_TEST := $(BUILD)/test_api
COVERAGE := coverage
WCT_COVERAGE_ROOT := $(CURDIR)/$(BUILD)/gcov-forks

ifneq ($(filter coverage,$(MAKECMDGOALS)),)
export WCT_COVERAGE_ROOT
endif

.PHONY: all clean clean-profiles test queue-oracle sanitize sanitizer-sentinels valgrind measure coverage

all: $(BIN)

$(BUILD):
	@mkdir -p $(BUILD) bin

$(BUILD)/wct.o: src/wct.c src/wct_internal.h include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g -c $< -o $@

$(BUILD)/wct_cli.o: tools/wct_cli.c src/wct_internal.h include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g -c $< -o $@

$(API_TEST): tests/test_api.c $(BUILD)/wct.o src/wct_internal.h include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g tests/test_api.c $(BUILD)/wct.o -o $@ $(LDFLAGS)

$(BIN): $(BUILD)/wct.o $(BUILD)/wct_cli.o
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test: all $(API_TEST)
	./tests/test_cli.sh
	./$(API_TEST)
	./tests/test_fuzz.sh

queue-oracle: all
	WCT_QUEUE_CFLAGS='$(CFLAGS)' WCT_QUEUE_LDFLAGS='$(LDFLAGS)' \
		./tests/test_queue_oracle.sh

sanitize: clean
	$(MAKE) CFLAGS='$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' all $(API_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./tests/test_cli.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(API_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./tests/test_fuzz.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
		$(MAKE) CFLAGS='$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' queue-oracle
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./tests/sanitizer_sentinels.sh

# Intentional failures are isolated from the product tests and must be
# classified by the sentinel harness rather than treated as product errors.
sanitizer-sentinels:
	./tests/sanitizer_sentinels.sh

valgrind: clean
	$(MAKE) CFLAGS='-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude' LDFLAGS='' all $(API_TEST)
	@if command -v valgrind >/dev/null 2>&1; then \
	valgrind --leak-check=full --error-exitcode=1 ./$(API_TEST); \
		valgrind --leak-check=full --error-exitcode=1 $(BIN) --model fixtures/smoke.model --mode state; \
		valgrind --leak-check=full --error-exitcode=1 $(BIN) --model fixtures/relation.model --mode relation --isolate --timeout-ms 100; \
		WCT_QUEUE_RUNNER='valgrind --leak-check=full --error-exitcode=1 --log-file=/dev/null' \
			$(MAKE) queue-oracle; \
	else \
		echo 'SKIP: valgrind not installed'; \
	fi

measure: all
	@test -n "$(OUT)" || (echo 'OUT is required, e.g. make measure OUT=evidence/iter-0/measure.tsv'; exit 2)
	@mkdir -p "$$(dirname '$(OUT)')"
	@printf 'command\twall_seconds\tuser_seconds\tsys_seconds\tmax_rss_kb\trun\n' > '$(OUT)'
	@set -eu; command='./bin/wise-combine-test --model $(or $(FIXTURE),fixtures/smoke.model) --mode $(or $(MODE),state)'; \
		i=1; while [ "$$i" -le "$(or $(REPEAT),3)" ]; do \
		LC_ALL=C /usr/bin/time -f "$$command\t%e\t%U\t%S\t%M\t$$i" -o '$(OUT).tmp' $$command >/dev/null; \
		cat '$(OUT).tmp' >> '$(OUT)'; i=$$((i + 1)); \
	done; rm -f '$(OUT).tmp'; LC_ALL=C awk -f tools/summarize_measure.awk '$(OUT)' > '$(OUT).summary.tsv'

coverage: clean
	$(MAKE) CFLAGS='$(CFLAGS) --coverage' \
		LDFLAGS='--coverage -Wl,--undefined=__gcov_dump' all $(API_TEST)
	mkdir -p $(BUILD)/gcov-forks
	./tests/test_cli.sh
	./$(API_TEST)
	./tests/test_fuzz.sh
	WCT_QUEUE_CFLAGS='$(CFLAGS) -w' \
		WCT_QUEUE_LDFLAGS='--coverage -Wl,--undefined=__gcov_dump' \
		./tests/test_queue_oracle.sh
	rm -f $(BUILD)/test_api.gcda
	tools/merge-coverage.sh $(BUILD) $(BUILD)/gcov-forks
	@mkdir -p $(COVERAGE)
	@set -e; \
		count=0; \
		for profile in $(BUILD)/*.gcda; do \
			[ -f "$$profile" ] || continue; \
			report='$(COVERAGE)/'$${profile##*/}'.gcov.txt'; \
			$(GCOV) -b -t "$$profile" -o $(BUILD) > "$$report"; \
			$(GCOV) -b -n "$$profile" -o $(BUILD) >> "$$report"; \
			count=$$((count + 1)); \
		done; \
		[ "$$count" -ge 2 ] || { \
			echo 'coverage failure: expected library and CLI profile data' >&2; \
			exit 1; \
		}; \
		for source in src/wct.c tools/wct_cli.c; do \
			grep -qF "File '$$source'" $(COVERAGE)/*.gcov.txt || { \
				echo "coverage failure: missing report for $$source" >&2; \
				exit 1; \
			}; \
		done; \
		{ \
			for report in $(COVERAGE)/*.gcov.txt; do \
				printf '=== %s ===\n' "$${report##*/}"; \
				grep -E '^(File|Lines executed|Branches executed|Taken at least once):' "$$report"; \
			done; \
		} > $(COVERAGE)/summary.txt; \
		rm -f $(BUILD)/test_api.gcda $(BUILD)/test_api.gcno \
			test_api.gcda test_api.gcno \
			queue_harness.gcda queue_harness.gcno; \
		printf 'coverage reports: %s\n' "$(COVERAGE)/summary.txt"

clean:
	@set -e; rm -rf $(COVERAGE); \
		find . -type f \( -name '*.gcda' -o -name '*.gcno' -o -name '*.gcov' \) \
			-exec rm -f -- {} +
	rm -rf $(BUILD) bin
