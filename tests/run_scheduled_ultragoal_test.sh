#!/bin/sh
set -eu

# Offline regression test for cron's minimal environment.  The fake runtime
# records argv, proving the runner invokes the pinned Node binary directly
# rather than relying on the `omx` env-based shim.
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
FAKE_NODE="$TMP/node"
FAKE_OMX="$TMP/omx.js"
ARGS="$TMP/args"
FAKE_CRONTAB="$TMP/crontab"
cat >"$FAKE_NODE" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" >"${WCT_TEST_ARGS:?}"
exit 0
EOF
chmod +x "$FAKE_NODE"
: >"$FAKE_OMX"
cat >"$FAKE_CRONTAB" <<'EOF'
#!/bin/sh
if [ "$1" = "-l" ]; then printf '%s\n' '0 10 13 9 * old-command # WCT_ULTRAGOAL_ONESHOT_20260913' '0 1 * * * keep-command'; exit 0; fi
cat >"${WCT_CRONTAB_CAPTURE:?}"
EOF
chmod +x "$FAKE_CRONTAB"

WCT_TEST_ARGS="$ARGS" WCT_NODE_BIN="$FAKE_NODE" WCT_OMX_JS="$FAKE_OMX" \
  WCT_CRONTAB_CAPTURE="$TMP/cron-out" PATH="$TMP:$PATH" HOME="$TMP/home" XDG_STATE_HOME="$TMP/state" \
  sh "$ROOT/tools/run-scheduled-ultragoal.sh"

[ "$(sed -n '1p' "$ARGS")" = "$FAKE_OMX" ]
[ "$(sed -n '2p' "$ARGS")" = "exec" ]
[ "$(sed -n '3p' "$ARGS")" = "-C" ]
[ "$(sed -n '4p' "$ARGS")" = "$ROOT" ]
! grep -q 'WCT_ULTRAGOAL_ONESHOT_20260913' "$TMP/cron-out"
grep -q 'keep-command' "$TMP/cron-out"

if WCT_NODE_BIN="$TMP/missing-node" WCT_OMX_JS="$FAKE_OMX" \
  HOME="$TMP/home" XDG_STATE_HOME="$TMP/state" \
  sh "$ROOT/tools/run-scheduled-ultragoal.sh" >/dev/null 2>&1; then
  echo "expected missing Node runtime to fail" >&2
  exit 1
fi

echo "scheduled runner tests: PASS"
