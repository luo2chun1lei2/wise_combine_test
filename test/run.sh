#!/usr/bin/env bash
set -euo pipefail

BIN=./build/wise_combine_test

file_out="$("$BIN" doc/examples/file-functions.dsl --max-length 3 --seed 0)"
echo "$file_out" | grep -q 'sequences: 21'

string_out="$("$BIN" doc/examples/string-functions.dsl --max-length 2 --seed 0)"
echo "$string_out" | grep -q 'OK'

machine_out="$("$BIN" doc/examples/connection.dsl --max-length 3)"
echo "$machine_out" | grep -q 'paths: 5'

events_out="$("$BIN" doc/examples/connection.dsl --events connect,connected_ok,disconnect,close --guard result=OK)"
echo "$events_out" | grep -q 'final: CLOSED'

cover_out="$("$BIN" doc/examples/connection.dsl --max-length 4 --cover --guard result=OK)"
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

nswitch_out="$("$BIN" doc/examples/connection.dsl --max-length 4 --n-switch 2 --guard result=OK)"
echo "$nswitch_out" | grep -q 'covered_nswitch_2: 7/7'

set +e
json_exec="$("$BIN" doc/examples/connection.dsl --events connect,close --json 2>&1)"
json_rc=$?
set -e
[ "$json_rc" -ne 0 ]
echo "$json_exec" | grep -q '"failed":true'

guard_ok="$("$BIN" doc/examples/connection.dsl --events connect,connected_ok,disconnect,close --guard result=OK)"
echo "$guard_ok" | grep -q 'final: CLOSED'

set +e
guard_fail="$("$BIN" doc/examples/connection.dsl --events connect,connected_ok --guard result=FAIL 2>&1)"
guard_fail_rc=$?
set -e
[ "$guard_fail_rc" -ne 0 ]
echo "$guard_fail" | grep -q 'no transition for event connected_ok'

set +e
guard_unbound="$("$BIN" doc/examples/guard.dsl --events go,ok 2>&1)"
guard_unbound_rc=$?
set -e
[ "$guard_unbound_rc" -ne 0 ]
echo "$guard_unbound" | grep -q "guard variable 'result' is unbound"

guard_events_ok="$("$BIN" doc/examples/guard.dsl --events go,ok --guard result=OK)"
echo "$guard_events_ok" | grep -q 'final: C'

guard_events_fail="$("$BIN" doc/examples/guard.dsl --events go,ok --guard result=FAIL)"
echo "$guard_events_fail" | grep -q 'final: D'

guard_paths_ok="$("$BIN" doc/examples/guard.dsl --max-length 3 --guard result=OK)"
echo "$guard_paths_ok" | grep -q 'B -ok-> C'
if echo "$guard_paths_ok" | grep -q 'B -ok-> D'; then exit 1; fi

guard_paths_unbound="$("$BIN" doc/examples/guard.dsl --max-length 3)"
echo "$guard_paths_unbound" | grep -q 'skipped_guards: 2'

guard_harness_ok="$("$BIN" doc/examples/guard.dsl --events go,ok --harness --guard result=OK)"
echo "$guard_harness_ok" | g++ -x c++ -o /tmp/wise_guard_ok -
/tmp/wise_guard_ok | grep -q 'ALL PASS'
rm -f /tmp/wise_guard_ok

guard_harness_unknown="$("$BIN" doc/examples/guard.dsl --events go,ok --harness --guard result=UNKNOWN)"
echo "$guard_harness_unknown" | g++ -x c++ -o /tmp/wise_guard_unknown -
set +e
guard_harness_unknown_out="$(/tmp/wise_guard_unknown 2>&1)"
guard_harness_unknown_rc=$?
set -e
rm -f /tmp/wise_guard_unknown
[ "$guard_harness_unknown_rc" -ne 0 ]
echo "$guard_harness_unknown_out" | grep -q 'FAIL: no transition'

dfs_out="$("$BIN" doc/examples/connection.dsl --max-length 3 --algorithm dfs)"
echo "$dfs_out" | grep -q '^paths: 5$'

if "$BIN" doc/examples/connection.dsl --max-length -1 >/dev/null 2>&1; then exit 1; fi
if "$BIN" doc/examples/connection.dsl --max-length 1x >/dev/null 2>&1; then exit 1; fi

duplicate_dsl="/tmp/wise_duplicate.dsl"
printf 'resource R { ctype: "int" states: A initial: A }\nresource R { ctype: "int" states: A initial: A }\n' > "$duplicate_dsl"
set +e
duplicate_out="$("$BIN" "$duplicate_dsl" 2>&1)"
duplicate_rc=$?
set -e
rm -f "$duplicate_dsl"
[ "$duplicate_rc" -ne 0 ]
echo "$duplicate_out" | grep -q 'duplicate resource: R'

bfs_limit="$("$BIN" doc/examples/file-functions.dsl --algorithm bfs --max-length 3 --max-cases 1)"
echo "$bfs_limit" | grep -q '^sequences: 1$'

tour_limit="$("$BIN" doc/examples/connection.dsl --algorithm tour --max-length 2)"
echo "$tour_limit" | grep -q 'tour truncated: true'

nested_harness_c="$("$BIN" doc/examples/nested.dsl --events power_on,start,stop,power_off --harness)"
echo "$nested_harness_c" | g++ -x c++ -o /tmp/wise_nested_harness -
/tmp/wise_nested_harness | grep -q 'ALL PASS'
rm -f /tmp/wise_nested_harness

pair_out="$("$BIN" doc/examples/file-functions.dsl --max-length 3 --coverage)"
echo "$pair_out" | grep -q 'covered_function_pairs:'

tway_out="$("$BIN" doc/examples/file-functions.dsl --max-length 3 --t-way 2)"
echo "$tway_out" | grep -q 'covered_tway_2:'

concurrent_paths="$("$BIN" doc/examples/concurrent.dsl --max-length 2)"
echo "$concurrent_paths" | grep -q 'paths: 4'

neg_sm="$("$BIN" doc/examples/connection.dsl --negative)"
echo "$neg_sm" | grep -q 'negative:'

deep_out="$("$BIN" doc/examples/deep-history.dsl --events power_on,to_b,power_off,resume_deep)"
echo "$deep_out" | grep -q 'final: SUB1_B'

shallow_hist="$("$BIN" doc/examples/deep-history.dsl --events power_on,to_b,power_off,resume_shallow)"
echo "$shallow_hist" | grep -q 'final: SUB1_A'

ref_count="$(python3 test/reference_check.py 3)"
tool_count="$("$BIN" doc/examples/connection.dsl --max-length 3 | grep '^paths: ' | awk '{print $2}')"
[ "$ref_count" = "$tool_count" ]

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
set +e
hjson_out="$(/tmp/hjson 2>&1)"
hjson_rc=$?
set -e
rm -f /tmp/hjson /tmp/a.txt /tmp/b.txt /tmp/c.txt
[ "$hjson_rc" -ne 0 ]
echo "$hjson_out" | grep -q '"kind":"failure"'

slow_c="$("$BIN" doc/examples/slow.dsl --max-length 1 --max-cases 1 --harness --timeout 1)"
echo "$slow_c" | gcc -x c - test/slow_sut.c -o /tmp/slow_harness -
set +e
slow_out="$(/tmp/slow_harness 2>&1)"
slow_rc=$?
set -e
rm -f /tmp/slow_harness
[ "$slow_rc" -ne 0 ]
echo "$slow_out" | grep -q 'TIMEOUT'

supplied_c="$("$BIN" doc/examples/observed.dsl --sequence 'open_h(a);close_h(0)' --harness)"
echo "$supplied_c" | gcc -x c - test/observed_sut.c -o /tmp/supplied_harness -
/tmp/supplied_harness | grep -q 'ALL PASS'
rm -f /tmp/supplied_harness

cpp_c="$("$BIN" doc/examples/cpp-class.dsl --max-length 2 --seed 0 --max-cases 3 --harness)"
echo "$cpp_c" | g++ -x c++ -Itest test/store_sut.cpp -o /tmp/cppharness -
/tmp/cppharness | grep -q 'ALL PASS'
rm -f /tmp/cppharness

sm_c="$("$BIN" doc/examples/sm-actions.dsl --events go,back --harness)"
echo "$sm_c" | g++ -x c++ -Itest test/worker_sut.cpp -o /tmp/smharness -
/tmp/smharness | grep -q 'ALL PASS'
rm -f /tmp/smharness

echo "PASS"
