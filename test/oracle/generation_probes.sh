#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ORACLE="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/build/wise_combine_test"
OUT="$ORACLE/generation-probes.tsv"

printf 'case\tmax_length\tgenerated\ttarget_index\textras\n' > "$OUT"

python3 - "$BIN" "$ORACLE" "$OUT" <<'PY'
import json
import subprocess
import sys

bin_path, oracle, out_path = sys.argv[1:4]
wanted = {
    "q1": "q_open,q_pop_empty",
    "q2": "q_open,q_push1,q_push2,q_pop_two,q_pop_one",
    "q3": "q_open,q_push1,q_close,q_reopen,q_size_nonempty",
    "q4": "q_open,q_push1,q_peek,q_size_one",
    "q5": "q_open,q_push1,q_push2,q_pop_two,q_pop_one,q_size_empty",
    "q6": "q_open,q_close,q_push_closed",
}
lengths = {"q1": 2, "q2": 5, "q3": 5, "q4": 4, "q5": 6, "q6": 3}

with open(out_path, "a", encoding="utf-8") as out:
    for case in ["q1", "q2", "q3", "q4", "q5", "q6"]:
        model = f"{oracle}/{case}.dsl"
        max_len = lengths[case]
        proc = subprocess.run(
            [bin_path, model, "--max-length", str(max_len), "--json"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
        data = json.loads(proc.stdout)
        seqs = [
            [part.split("(", 1)[0].strip() for part in seq.split(" ; ")]
            for seq in data.get("sequences", [])
        ]
        target = wanted[case].split(",")
        index = next((i for i, seq in enumerate(seqs) if seq == target), None)
        if index is None:
            raise SystemExit(f"{case}: target sequence not discovered")
        extras = len(seqs) - 1
        out.write(f"{case}\t{max_len}\t{len(seqs)}\t{index}\t{extras}\n")
PY

cat "$OUT"
echo "generation probes passed: all six target sequences were discovered by the generator"
