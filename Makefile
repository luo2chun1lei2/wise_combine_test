CC ?= cc
CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude
LDFLAGS ?=
BUILD := build
BIN := bin/wise-combine-test
API_TEST := $(BUILD)/test_api

.PHONY: all clean test sanitize sanitizer-sentinels valgrind measure coverage

all: $(BIN)

$(BUILD):
	@mkdir -p $(BUILD) bin

$(BUILD)/wct.o: src/wct.c src/wct_internal.h include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g -c $< -o $@

$(BUILD)/wct_cli.o: tools/wct_cli.c src/wct_internal.h include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g -c $< -o $@

$(API_TEST): tests/test_api.c src/wct.c src/wct_internal.h include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g tests/test_api.c src/wct.c -o $@ $(LDFLAGS)

$(BIN): $(BUILD)/wct.o $(BUILD)/wct_cli.o
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test: all $(API_TEST)
	./tests/test_cli.sh
	./$(API_TEST)
	./tests/test_fuzz.sh

sanitize: clean
	$(MAKE) CFLAGS='$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' all $(API_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./tests/test_cli.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(API_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./tests/test_fuzz.sh
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
	$(MAKE) CFLAGS='$(CFLAGS) --coverage' LDFLAGS='--coverage' all $(API_TEST)
	./tests/test_cli.sh
	./$(API_TEST)
	./tests/test_fuzz.sh
	@mkdir -p coverage
	@gcov -b -c -o build src/wct.c > coverage/wct.gcov.txt
	@printf 'coverage report: coverage/wct.gcov.txt\n'

clean:
	rm -rf $(BUILD) bin
