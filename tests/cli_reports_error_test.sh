#!/usr/bin/env bash
set -u
binary="$1"
spec="$2"
adapter="$3"
reports_file="$(mktemp)"
trap 'unlink "$reports_file"' EXIT
set +e
output="$("$binary" run "$spec" --adapter "$adapter" --reports "$reports_file" --run-id invalid 2>&1)"
status=$?
set -e
if [[ "$status" -ne 5 ]]; then
  echo "expected exit 5 for a regular-file reports path, got $status" >&2
  exit 1
fi
if [[ "$output" != *"unable to create reports directory"* ]]; then
  echo "missing reports-directory diagnostic: $output" >&2
  exit 1
fi
