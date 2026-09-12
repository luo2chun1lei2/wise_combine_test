#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TOOL="/home/workspace_data/works/myprojects/wise_combine_test.plan_goal/src/out/wise_combine_test"
OUT="$HERE/results/probes.txt"
{
  echo '[cycle max-depth=8 max-flows=100]'
  "$TOOL" "$HERE/probes.ct" --dry-run --max-depth 8 --max-flows 100 --report text
  echo '[cycle max-flows=1]'
  "$TOOL" "$HERE/probes.ct" --dry-run --max-depth 8 --max-flows 1 --report text
  echo '[constraints]'
  "$TOOL" "$HERE/constraints.ct" --dry-run --max-depth 8 --max-flows 100 --report text
  echo '[missing required parameter binding]'
  "$TOOL" "$HERE/missing_parameter.ct" --dry-run --max-depth 8 --max-flows 100 --report text
  echo '[non-self cycle]'
  "$TOOL" "$HERE/nonself_cycle.ct" --dry-run --max-depth 8 --max-flows 100 --report text
  echo '[missing adapter protocol whitespace]'
  "$TOOL" "$HERE/Q1.ct" --adapter /bin/sh --adapter-arg -c --adapter-arg 'read x; printf %s\\n '\''{ "protocol": 1, "status": "ok", "return": 0, "returns": {}, "stdout": "", "stderr": ""}'\''' --report json
} > "$OUT" 2>&1 || true
cat "$OUT"
