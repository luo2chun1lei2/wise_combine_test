#!/bin/sh
set -eu
asset=/home/workspace_data/works/myprojects/wise_combine_test-review-2-assets/omx
set +e
"$asset/bin/queue_harness_m0" "$asset/models/probe-cycle.model" > "$asset/probe-cycle.log" 2>&1
cycle_rc=$?
"$asset/bin/queue_harness_m0" "$asset/models/probe-subset.model" > "$asset/probe-subset.log" 2>&1
subset_rc=$?
"$asset/bin/queue_harness_m0" "$asset/models/probe-repeat.model" > "$asset/probe-repeat.log" 2>&1
repeat_rc=$?
set -e
printf 'cycle_rc=%s\nsubset_rc=%s\nrepeat_rc=%s\n' "$cycle_rc" "$subset_rc" "$repeat_rc" > "$asset/generation-probes.txt"
cat "$asset/probe-cycle.log" "$asset/probe-subset.log" "$asset/probe-repeat.log" >> "$asset/generation-probes.txt"
