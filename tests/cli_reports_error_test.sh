#!/usr/bin/env bash
set -u
binary="$1"
spec="$2"
adapter="$3"
reports_file="$(mktemp)"
trap 'rmdir "$reports_file" 2>/dev/null || true' EXIT
set +e
output="$($binary run "$spec" --adapter "$adapter" --reports "$reports_file" --run-id invalid 2>&1)"
status=$?
set -e
if [[ "$status" -eq 0 ]]; then
  echo "expected nonzero status for a regular-file reports path" >&2
  exit 1
fi
if [[ "$output" != *"unable to create reports directory"* ]]; then
  echo "missing reports-directory diagnostic: $output" >&2
  exit 1
fi
