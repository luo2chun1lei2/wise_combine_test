#!/usr/bin/env bash
set -euo pipefail
binary="$1"
for command in --help help; do
  output="$($binary "$command")"
  [[ "$output" == *"verify-report-v2"* && "$output" == *"replay REPORT"* ]]
done
if "$binary" unknown >/dev/null 2>&1; then exit 1; fi
