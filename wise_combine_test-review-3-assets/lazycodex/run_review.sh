#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
PROJECT=/home/workspace_data/works/myprojects/wise_combine_test.lazycodex
COMMON=/home/workspace_data/works/myprojects/wise_combine_test-review-2-assets/common
TMP=/tmp/lazycodex-review-adapters
BIN="$TMP/tests/fixtures/bin"
BUILD="$ROOT/build"
REPORTS="$ROOT/reports"
rm -rf "$TMP" "$REPORTS"
mkdir -p "$BIN" "$REPORTS"

for mutant in 0 1 2 3 4 5 6; do
  c++ -std=c++17 -Wall -Wextra -Wpedantic -DMUTANT="$mutant" \
    -I"$COMMON" "$ROOT/queue_adapter.cpp" -o "$BIN/adapter_mutant_$mutant"
done

printf 'scenario,mutant,repeat,expected,actual,exit,case_count,passed,failed\n' > "$ROOT/results.csv"
for scenario in 1 2 3 4 5 6; do
  for mutant in 0 "$scenario"; do
    for repeat in 1 2; do
      state="$TMP/state.bin"
      rm -f "$state"
      out="$REPORTS/q${scenario}-m${mutant}-r${repeat}"
      mkdir -p "$out"
      set +e
      "$PROJECT/build/wise-combine" run "$ROOT/q${scenario}.json" \
        --adapter "$BIN/adapter_mutant_$mutant" --arg --state --arg "$state" \
        --reports "$out" --run-id result > "$out/stdout.log" 2> "$out/stderr.log"
      rc=$?
      set -e
      summary="$out/result-summary.json"
      case_count=$(sed -n 's/.*"case_count":\([0-9][0-9]*\).*/\1/p' "$summary")
      passed=$(sed -n 's/.*"passed":\([0-9][0-9]*\).*/\1/p' "$summary")
      failed=$(sed -n 's/.*"failed":\([0-9][0-9]*\).*/\1/p' "$summary")
      if [ "$mutant" -eq 0 ]; then expected=clean-pass; else expected=mutant-fail; fi
      if [ "$rc" -eq 0 ]; then actual=pass; else actual=fail; fi
      printf 'Q%s,%s,%s,%s,%s,%s,%s,%s,%s\n' "$scenario" "$mutant" "$repeat" "$expected" "$actual" "$rc" "$case_count" "$passed" "$failed" >> "$ROOT/results.csv"
    done
  done
done

{
  echo '# LazyCodex Q1-Q6 run'
  echo
  echo 'The CSV records 24 independent executions. Clean runs are mutant 0; negative runs use the matching single mutant.'
  echo
  cat "$ROOT/results.csv"
} > "$ROOT/run-log.md"
