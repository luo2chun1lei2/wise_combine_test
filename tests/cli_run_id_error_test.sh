#!/usr/bin/env bash
set -u
binary="$1"; spec="$2"; adapter="$3"; reports="$(mktemp -d)"
mkdir "$reports/inside"
trap 'for f in "$reports/inside"/* "$reports"/*; do test -f "$f" && unlink "$f"; done; rmdir "$reports/inside" "$reports"' EXIT
set +e
output=$("$binary" run "$spec" --adapter "$adapter" --reports "$reports/inside" --run-id ../escape 2>&1)
status=$?
set -e
if [[ "$status" -ne 6 || "$output" != *"usage:"* ]]; then
  echo "expected invalid run-id usage error, got $status: $output" >&2
  exit 1
fi
