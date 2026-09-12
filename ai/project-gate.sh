#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

python3 -m json.tool ai/ai-usage-log.json >/dev/null
python3 -m json.tool ai/defects.json >/dev/null

python3 - "$ROOT/ai/defects.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as f:
    data = json.load(f)

blocked = [
    d for d in data["defects"]
    if d["status"] == "open" and d["severity"] in {"high", "blocker"}
]
if blocked:
    for d in blocked:
        print(f"blocking defect: {d['id']} {d['title']}", file=sys.stderr)
    sys.exit(1)
PY

make check

if [ "${1:-}" = "--asan" ]; then
    make asan
fi

echo "project gate passed"
