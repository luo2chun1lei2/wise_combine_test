CC ?= cc
CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude
LDFLAGS ?=
BUILD := build
BIN := bin/wise-combine-test
API_TEST := $(BUILD)/test_api

.PHONY: all clean test sanitize valgrind measure

all: $(BIN)

$(BUILD):
	@mkdir -p $(BUILD) bin

$(BUILD)/wct.o: src/wct.c include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g -c $< -o $@

$(BUILD)/wct_cli.o: tools/wct_cli.c include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g -c $< -o $@

$(API_TEST): tests/test_api.c src/wct.c include/wct.h | $(BUILD)
	$(CC) $(CFLAGS) -g tests/test_api.c src/wct.c -o $@ $(LDFLAGS)

$(BIN): $(BUILD)/wct.o $(BUILD)/wct_cli.o
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test: all $(API_TEST)
	./tests/test_cli.sh
	./$(API_TEST)

sanitize: clean
	$(MAKE) CFLAGS='$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' all $(API_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./tests/test_cli.sh
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./$(API_TEST)

valgrind: all
	@if command -v valgrind >/dev/null 2>&1; then valgrind --leak-check=full --error-exitcode=1 $(BIN) --model fixtures/smoke.model --mode state; else echo 'SKIP: valgrind not installed'; fi

measure: all
	@test -n "$(OUT)" || (echo 'OUT is required, e.g. make measure OUT=evidence/iter-0/measure.tsv'; exit 2)
	@mkdir -p "$$(dirname '$(OUT)')"
	@printf 'command\twall_seconds\tmax_rss_kb\n' > '$(OUT)'
	@/usr/bin/time -f './bin/wise-combine-test --model $(or $(FIXTURE),fixtures/smoke.model) --mode state\t%e\t%M' -o '$(OUT).tmp' ./bin/wise-combine-test --model $(or $(FIXTURE),fixtures/smoke.model) --mode state >/dev/null
	@cat '$(OUT).tmp' >> '$(OUT)'; rm -f '$(OUT).tmp'

clean:
	rm -rf $(BUILD) bin
