#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PROMPT="$ROOT/.omx/scheduled/ultragoal-20260913.md"
STAMP=$(date '+%Y%m%d-%H%M%S')
LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/wise-combine-test/scheduled"
mkdir -p "$LOG_DIR"

# Do not execute the `omx` shim here: it has a `/usr/bin/env node` shebang,
# and cron's minimal PATH can resolve that to an old system Node (the 9/13
# run failed while parsing ESM). Invoke the pinned Node binary and CLI module
# directly, while retaining the pinned bin directory first in PATH for child
# commands.  These paths are overridable for an offline harness only.
NODE_BIN=${WCT_NODE_BIN:-/home/hpvr/.nvm/versions/node/v22.23.2/bin/node}
OMX_JS=${WCT_OMX_JS:-/home/hpvr/.nvm/versions/node/v22.23.2/lib/node_modules/oh-my-codex/dist/cli/omx.js}
if [ ! -x "$NODE_BIN" ]; then
  echo "scheduled ultragoal: Node runtime not executable: $NODE_BIN" >&2
  exit 127
fi
if [ ! -f "$OMX_JS" ]; then
  echo "scheduled ultragoal: OMX CLI not found: $OMX_JS" >&2
  exit 127
fi
PATH="$(dirname "$NODE_BIN"):$PATH"
export PATH

# Remove the exact one-shot entry before starting. This prevents the September
# 13 schedule from recurring in future years while leaving unrelated jobs intact.
MARKER='# WCT_ULTRAGOAL_ONESHOT_20260913'
if command -v crontab >/dev/null 2>&1; then
  current=$(crontab -l 2>/dev/null || :)
  printf '%s\n' "$current" | awk -v marker="$MARKER" '$0 != marker && index($0, marker) == 0' | crontab - || :
fi

exec "$NODE_BIN" "$OMX_JS" exec \
  -C "$ROOT" --ask-for-approval never -s danger-full-access --json \
  -o "$LOG_DIR/ultragoal-$STAMP.last-message" \
  "$(cat "$PROMPT")" >"$LOG_DIR/ultragoal-$STAMP.jsonl" 2>&1
