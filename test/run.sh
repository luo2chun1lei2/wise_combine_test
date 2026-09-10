#!/usr/bin/env bash
set -euo pipefail

BIN=./build/wise_combine_test

file_out="$("$BIN" doc/examples/file-functions.dsl 3 0)"
echo "$file_out" | grep -q 'sequences: 21'

string_out="$("$BIN" doc/examples/string-functions.dsl 2 0)"
echo "$string_out" | grep -q 'OK'

echo "PASS"
