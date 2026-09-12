#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
COMMON="$HERE/../common"
TOOL="/home/workspace_data/works/myprojects/wise_combine_test.plan_goal/src/out/wise_combine_test"
mkdir -p "$HERE/bin" "$HERE/results"
for mutant in 0 1 2 3 4 5 6; do
  g++ -std=c++17 -O2 -Wall -Wextra -DMUTANT=$mutant -I"$COMMON" "$HERE/adapter.cpp" -o "$HERE/bin/adapter-$mutant"
done
: > "$HERE/results/summary.tsv"
printf 'run\tcase\tvariant\trepeat\texit\tpassed\tfailed\tverdict\n' >> "$HERE/results/summary.tsv"
run=0
for case_id in 1 2 3 4 5 6; do
  for variant in clean mutant; do
    if [ "$variant" = clean ]; then mutant=0; expected=pass; else mutant=$case_id; expected=fail; fi
    for repeat in 1 2; do
      run=$((run + 1))
      state="$HERE/results/Q${case_id}-${variant}-${repeat}.state"
      output="$HERE/results/Q${case_id}-${variant}-${repeat}.json"
      errors="$HERE/results/Q${case_id}-${variant}-${repeat}.stderr"
      rm -f "$state"
      set +e
      "$TOOL" "$HERE/Q${case_id}.ct" --adapter "$HERE/bin/adapter-$mutant" --adapter-arg "$state" --max-depth 8 --max-flows 100 --report json >"$output" 2>"$errors"
      rc=$?
      set -e
      passed=$(sed -n 's/.*"passed": \([0-9][0-9]*\).*/\1/p' "$output" | head -1)
      failed=$(sed -n 's/.*"failed": \([0-9][0-9]*\).*/\1/p' "$output" | head -1)
      verdict=FAIL
      if [ "$expected" = pass ] && [ "${failed:-x}" = 0 ] && [ "${passed:-0}" -gt 0 ]; then verdict=PASS; fi
      if [ "$expected" = fail ] && [ "${failed:-0}" -gt 0 ]; then verdict=PASS; fi
      printf '%s\tQ%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$run" "$case_id" "$variant" "$repeat" "$rc" "${passed:-NA}" "${failed:-NA}" "$verdict" >> "$HERE/results/summary.tsv"
    done
  done
done
cat "$HERE/results/summary.tsv"
