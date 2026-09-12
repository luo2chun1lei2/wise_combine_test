#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
COMMON="$ROOT/../common"
PROJ="/home/workspace_data/works/myprojects/wise_combine_test.manual"
BIN="$PROJ/build/wise_combine_test"
OUT="$ROOT/results.tsv"
printf 'run\tcase\tmutant\tstatus\tlog\n' > "$OUT"
declare -A lengths=([q1]=2 [q2]=5 [q3]=5 [q4]=4 [q5]=6 [q6]=3)
declare -A wanted
wanted[q1]='q_open,q_pop_empty'
wanted[q2]='q_open,q_push1,q_push2,q_pop_two,q_pop_one'
wanted[q3]='q_open,q_push1,q_close,q_reopen,q_size_nonempty'
wanted[q4]='q_open,q_push1,q_peek,q_size_one'
wanted[q5]='q_open,q_push1,q_push2,q_pop_two,q_pop_one,q_size_empty'
wanted[q6]='q_open,q_close,q_push_closed'
for round in 1 2; do
  for case in q1 q2 q3 q4 q5 q6; do
    for mutant in 0 "$((10#${case#q}))"; do
      work="$ROOT/${case}-m${mutant}-r${round}"
      rm -rf "$work"
      mkdir -p "$work"
      cp "$ROOT/queue_adapter.c" "$work/queue_adapter.c"
      cp "$ROOT/${case}.dsl" "$work/model.dsl"
      gcc -std=c11 -Wall -Wextra -DMUTANT="$mutant" -I"$COMMON" -c "$work/queue_adapter.c" -o "$work/queue_adapter.o" 2>"$work/compile.log"
      "$BIN" "$work/model.dsl" --max-length "${lengths[$case]}" --max-cases 100 --json >"$work/generated.json" 2>"$work/generate.log"
      idx=$(python3 -c 'import json,sys; want=sys.argv[1].split(","); d=json.load(open(sys.argv[2])); print(next(i for i,s in enumerate(d["sequences"]) if [x.split("(",1)[0].strip() for x in s.split(" ; ")]==want))' "${wanted[$case]}" "$work/generated.json")
      "$BIN" "$work/model.dsl" --max-length "${lengths[$case]}" --replay "$idx" --harness-json >"$work/harness.c" 2>>"$work/generate.log"
      gcc -std=c11 -Wall -Wextra "$work/harness.c" "$work/queue_adapter.o" -o "$work/test" 2>"$work/harness-compile.log"
      set +e
      timeout 10s "$work/test" >"$work/result.log" 2>&1
      rc=$?
      set -e
      if [[ "$mutant" == 0 ]]; then expected=0; else expected=1; fi
      if [[ "$rc" == "$expected" ]]; then status=PASS; else status=FAIL; fi
      printf '%s\t%s\t%s\t%s\t%s\n' "$round" "$case" "$mutant" "$status" "$work/result.log" >> "$OUT"
    done
  done
done
cat "$OUT"
