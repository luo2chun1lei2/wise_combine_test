#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ORACLE="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/build/wise_combine_test"
OUT="$ORACLE/results.tsv"

declare -A SEQUENCES
declare -A MUTANTS

SEQUENCES[q1]='q_open();q_pop_empty(0)'
SEQUENCES[q2]='q_open();q_push1(0);q_push2(0);q_pop_two(0);q_pop_one(0)'
SEQUENCES[q3]='q_open();q_push1(0);q_close(0);q_reopen(0);q_size_nonempty(0)'
SEQUENCES[q4]='q_open();q_push1(0);q_peek(0);q_size_one(0)'
SEQUENCES[q5]='q_open();q_push1(0);q_push2(0);q_pop_two(0);q_pop_one(0);q_size_empty(0)'
SEQUENCES[q6]='q_open();q_close(0);q_push_closed(0)'

MUTANTS[q1]=1
MUTANTS[q2]=2
MUTANTS[q3]=3
MUTANTS[q4]=4
MUTANTS[q5]=5
MUTANTS[q6]=6

printf 'round\tcase\tmutant\tstatus\n' > "$OUT"

for case in q1 q2 q3 q4 q5 q6; do
  for round in 1 2; do
    for mutant in 0 "${MUTANTS[$case]}"; do
      work="$(mktemp -d)"
      gcc -std=c11 -Wall -Wextra -DMUTANT="$mutant" -I"$ORACLE" \
        -c "$ORACLE/queue_adapter.c" -o "$work/adapter.o"
      "$BIN" "$ORACLE/$case.dsl" --sequence "${SEQUENCES[$case]}" --harness \
        > "$work/harness.c"
      gcc -std=c11 -Wall -Wextra "$work/harness.c" "$work/adapter.o" -o "$work/test"

      set +e
      timeout 10s "$work/test" > "$work/result.log" 2>&1
      rc=$?
      set -e

      if [[ "$mutant" == 0 ]]; then
        expected=0
      else
        expected=1
      fi

      if [[ "$rc" == "$expected" ]]; then
        status=PASS
      else
        status=FAIL
      fi

      printf '%s\t%s\t%s\t%s\n' "$round" "$case" "$mutant" "$status" >> "$OUT"
      rm -rf "$work"
    done
  done
done

cat "$OUT"
if grep -q '	FAIL' "$OUT"; then
  echo "unified oracle failed" >&2
  exit 1
fi
echo "unified oracle passed"
