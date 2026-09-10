#!/usr/bin/env bash
set -euo pipefail

BIN=./build/wise_combine_test

file_out="$("$BIN" doc/examples/file-functions.dsl --max-length 3 --seed 0)"
echo "$file_out" | grep -q 'sequences: 21'

string_out="$("$BIN" doc/examples/string-functions.dsl --max-length 2 --seed 0)"
echo "$string_out" | grep -q 'OK'

machine_out="$("$BIN" doc/examples/connection.dsl --max-length 3)"
echo "$machine_out" | grep -q 'paths: 7'

events_out="$("$BIN" doc/examples/connection.dsl --events connect,connected_ok,disconnect,close)"
echo "$events_out" | grep -q 'final: CLOSED'

harness_c="$("$BIN" doc/examples/file-functions.dsl --max-length 2 --seed 42 --max-cases 3 --harness)"
echo "$harness_c" | gcc -x c - -o /tmp/wise_harness
(
  cd /tmp
  /tmp/wise_harness
) | grep -q 'ALL PASS'
rm -f /tmp/wise_harness /tmp/a.txt /tmp/b.txt /tmp/c.txt

observed_c="$("$BIN" doc/examples/observed.dsl --max-length 2 --seed 0 --max-cases 4 --harness)"
echo "$observed_c" | gcc -x c - test/observed_sut.c -o /tmp/observed_harness
/tmp/observed_harness | grep -q 'ALL PASS'
rm -f /tmp/observed_harness

echo "PASS"
