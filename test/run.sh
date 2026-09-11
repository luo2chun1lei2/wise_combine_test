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

cover_out="$("$BIN" doc/examples/connection.dsl --max-length 4 --cover)"
echo "$cover_out" | grep -q 'covered transitions: 5/5'

nested_out="$("$BIN" doc/examples/nested.dsl --events power_on,start,stop,power_off)"
echo "$nested_out" | grep -q 'final: OFF'

history_out="$("$BIN" doc/examples/nested-history.dsl --events power_on,start,power_off,resume)"
echo "$history_out" | grep -q 'final: ON_WORKING'

concurrent_out="$("$BIN" doc/examples/concurrent.dsl --events power_on,a_next,b_next,power_off)"
echo "$concurrent_out" | grep -q 'final: OFF'

bfs_out="$("$BIN" doc/examples/file-functions.dsl --algorithm bfs --max-length 2)"
echo "$bfs_out" | grep -q 'sequences: 5'

negative_out="$("$BIN" doc/examples/file-functions.dsl --max-length 3 --negative)"
echo "$negative_out" | grep -q 'negative: 6'

nswitch_out="$("$BIN" doc/examples/connection.dsl --max-length 4 --n-switch 2)"
echo "$nswitch_out" | grep -q 'covered_nswitch_2: 7/7'

json_exec="$("$BIN" doc/examples/connection.dsl --events connect,close --json || true)"
echo "$json_exec" | grep -q '"failed":true'

guard_ok="$("$BIN" doc/examples/connection.dsl --events connect,connected_ok,disconnect,close --guard result=OK)"
echo "$guard_ok" | grep -q 'final: CLOSED'

guard_fail="$("$BIN" doc/examples/connection.dsl --events connect,connected_ok --guard result=FAIL || true)"
echo "$guard_fail" | grep -q 'no transition for event connected_ok'

pair_out="$("$BIN" doc/examples/file-functions.dsl --max-length 3 --coverage)"
echo "$pair_out" | grep -q 'covered_function_pairs:'

concurrent_paths="$("$BIN" doc/examples/concurrent.dsl --max-length 2)"
echo "$concurrent_paths" | grep -q 'paths: 4'

neg_sm="$("$BIN" doc/examples/connection.dsl --negative)"
echo "$neg_sm" | grep -q 'negative:'

deep_out="$("$BIN" doc/examples/deep-history.dsl --events power_on,to_b,power_off,resume_deep)"
echo "$deep_out" | grep -q 'final: SUB1_B'

shallow_hist="$("$BIN" doc/examples/deep-history.dsl --events power_on,to_b,power_off,resume_shallow)"
echo "$shallow_hist" | grep -q 'final: SUB1_A'

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

gcc -shared -fPIC test/observed_sut.c -o /tmp/observed.so
dylib_c="$("$BIN" doc/examples/observed.dsl --max-length 2 --seed 0 --max-cases 4 --dylib)"
echo "$dylib_c" | gcc -x c - -ldl -o /tmp/observed_dylib
/tmp/observed_dylib /tmp/observed.so | grep -q 'ALL PASS'
rm -f /tmp/observed.so /tmp/observed_dylib

hjson_c="$("$BIN" doc/examples/file-functions.dsl --max-length 1 --seed 0 --max-cases 1 --harness-json)"
echo "$hjson_c" | gcc -x c - -o /tmp/hjson
/tmp/hjson | grep -q '"kind":"failure"' || true
rm -f /tmp/hjson /tmp/a.txt /tmp/b.txt /tmp/c.txt

echo "PASS"
