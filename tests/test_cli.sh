#!/bin/sh
set -eu

bin=./bin/wise-combine-test
[ -s "$bin" ]
$bin --help >/dev/null
$bin --version | grep -q 'schema 1'
$bin --model fixtures/smoke.model --mode state | grep -q 'steps=1 covered=1 failures=0'
$bin --model fixtures/relation.model --mode relation | grep -q 'steps=2 covered=2 failures=0'
$bin --model fixtures/smoke.model --mode state --isolate --timeout-ms 100 | grep -q 'process_exit=0'
$bin --model fixtures/relation.model --mode relation --isolate --timeout-ms 100 | grep -q 'process_exit=0'
$bin --model fixtures/function_relations.model --mode relation --trace /tmp/wct-test-trace.$$ >/dev/null
$bin --replay /tmp/wct-test-trace.$$ | grep -q 'replay=PASS'
sed 's/model_digest .*/model_digest 0000000000000000/' /tmp/wct-test-trace.$$ > /tmp/wct-test-trace-tampered.$$
sed '/^model /p' /tmp/wct-test-trace.$$ > /tmp/wct-test-trace-duplicate.$$
sed 's/^mode relation$/mode relation EXTRA/' /tmp/wct-test-trace.$$ > /tmp/wct-test-trace-trailing.$$
if $bin --replay /tmp/wct-test-trace-trailing.$$ >/dev/null 2>&1; then echo 'trailing singleton data unexpectedly replayed' >&2; exit 1; fi
if $bin --replay /tmp/wct-test-trace-duplicate.$$ >/dev/null 2>&1; then echo 'duplicate model trace unexpectedly replayed' >&2; exit 1; fi
sed 's/edge fetch->transform/edge altered->transform/' /tmp/wct-test-trace.$$ > /tmp/wct-test-trace-edge-tampered.$$
if $bin --replay /tmp/wct-test-trace-edge-tampered.$$ >/dev/null 2>&1; then
    echo 'altered edge metadata unexpectedly replayed' >&2
    exit 1
fi
if $bin --replay /tmp/wct-test-trace-tampered.$$ >/dev/null 2>&1; then
    echo 'tampered trace unexpectedly replayed' >&2
    exit 1
fi
cp /tmp/wct-test-trace.$$ /tmp/wct-test-trace-garbage.$$
echo 'garbage blah' >> /tmp/wct-test-trace-garbage.$$
if $bin --replay /tmp/wct-test-trace-garbage.$$ >/dev/null 2>&1; then echo 'garbage trace unexpectedly replayed' >&2; exit 1; fi
rm -f /tmp/wct-test-trace.$$ /tmp/wct-test-trace-edge-tampered.$$
rm -f /tmp/wct-test-trace-tampered.$$
if $bin --model fixtures/smoke.model --mode invalid >/dev/null 2>&1; then
    echo 'invalid mode unexpectedly succeeded' >&2
    exit 1
fi
if $bin --model fixtures/missing.model >/dev/null 2>&1; then
    echo 'missing model unexpectedly succeeded' >&2
    exit 1
fi
if $bin --model fixtures/smoke.model --isolate --timeout-ms 2147483648 >/dev/null 2>&1; then
    echo 'timeout overflow unexpectedly accepted' >&2
    exit 1
fi
space_dir="/tmp/wct model.$$"
space_trace="/tmp/wct-space-trace.$$"
mkdir -p "$space_dir"
cp fixtures/smoke.model "$space_dir/model.txt"
if $bin --model "$space_dir/model.txt" --trace "$space_trace" >/dev/null 2>&1; then
    echo 'trace unexpectedly accepted a model path containing whitespace' >&2
    rm -rf "$space_dir" "$space_trace"
    exit 1
fi
rm -rf "$space_dir" "$space_trace"
echo 'CLI smoke tests passed'
