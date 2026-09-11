#!/bin/sh
set -eu

bin=./bin/wise-combine-test
[ -s "$bin" ]
$bin --help >/dev/null
$bin --version | grep -q 'schema 1'
$bin --model fixtures/smoke.model --mode state | grep -q 'steps=1 covered=1 failures=0'
$bin --model fixtures/relation.model --mode relation | grep -q 'steps=2 covered=2 failures=0'
$bin --model fixtures/function_relations.model --mode relation --trace /tmp/wct-test-trace.$$ >/dev/null
$bin --replay /tmp/wct-test-trace.$$ | grep -q 'replay=PASS'
sed 's/model_digest .*/model_digest 0000000000000000/' /tmp/wct-test-trace.$$ > /tmp/wct-test-trace-tampered.$$
if $bin --replay /tmp/wct-test-trace-tampered.$$ >/dev/null 2>&1; then
    echo 'tampered trace unexpectedly replayed' >&2
    exit 1
fi
rm -f /tmp/wct-test-trace.$$
rm -f /tmp/wct-test-trace-tampered.$$
if $bin --model fixtures/smoke.model --mode invalid >/dev/null 2>&1; then
    echo 'invalid mode unexpectedly succeeded' >&2
    exit 1
fi
if $bin --model fixtures/missing.model >/dev/null 2>&1; then
    echo 'missing model unexpectedly succeeded' >&2
    exit 1
fi
echo 'CLI smoke tests passed'
