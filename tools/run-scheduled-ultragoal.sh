#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PROMPT="$ROOT/.omx/scheduled/ultragoal-20260913.md"
STAMP=$(date '+%Y%m%d-%H%M%S')
LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/wise-combine-test/scheduled"
mkdir -p "$LOG_DIR"

exec /home/hpvr/.nvm/versions/node/v22.23.2/bin/omx exec \
  -C "$ROOT" --ask-for-approval never -s danger-full-access --json \
  -o "$LOG_DIR/ultragoal-$STAMP.last-message" \
  "$(cat "$PROMPT")" >"$LOG_DIR/ultragoal-$STAMP.jsonl" 2>&1
