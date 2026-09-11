#!/bin/sh
# Run deliberate memory faults in throw-away binaries.  These are not product
# tests: success means the sanitizer reported the expected class of fault.
set -eu

cc=${CC:-cc}
tmp=${TMPDIR:-/tmp}/wct-sanitizer-sentinels.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"
flags='-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'

cat >"$tmp/oob.c" <<'EOF'
#include <stddef.h>
int main(void) { volatile int a[1] = {0}; a[2] = 1; return a[0]; }
EOF
cat >"$tmp/leak.c" <<'EOF'
#include <stdlib.h>
int main(void) { (void)malloc(64); return 0; }
EOF

"$cc" $flags "$tmp/oob.c" -o "$tmp/oob"
"$cc" $flags "$tmp/leak.c" -o "$tmp/leak"

run_expected() {
    name=$1; pattern=$2; shift 2
    out="$tmp/$name.log"
    set +e
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
      UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "$@" >"$out" 2>&1
    rc=$?
    set -e
    if ! grep -Eiq "$pattern" "$out"; then
        echo "SANITIZER SENTINEL FAIL: $name (diagnostic not found)" >&2
        cat "$out" >&2
        return 1
    fi
    # OOB terminates non-zero; LeakSanitizer may return zero after reporting.
    case "$name" in
        oob) [ "$rc" -ne 0 ] || { echo "SANITIZER SENTINEL FAIL: oob exited 0" >&2; return 1; } ;;
    esac
    printf 'SANITIZER SENTINEL PASS: %s (expected %s, exit=%s)\n' "$name" "$pattern" "$rc"
}

run_expected oob 'AddressSanitizer|runtime error' "$tmp/oob"
run_expected leak 'LeakSanitizer|detected memory leaks' "$tmp/leak"
