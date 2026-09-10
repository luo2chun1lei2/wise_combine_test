#!/usr/bin/env bash
set -euo pipefail

BIN=./build/wise_combine_test

file_out="$("$BIN" doc/examples/file-functions.dsl --max-length 3 --seed 0)"
echo "$file_out" | grep -q 'sequences: 21'

string_out="$("$BIN" doc/examples/string-functions.dsl --max-length 2 --seed 0)"
echo "$string_out" | grep -q 'OK'

machine_out="$("$BIN" doc/examples/connection.dsl --max-length 3)"
echo "$machine_out" | grep -q 'paths: 7'

echo "PASS"
