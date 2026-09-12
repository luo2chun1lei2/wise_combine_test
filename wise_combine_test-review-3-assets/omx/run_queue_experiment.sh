#!/bin/sh
set -eu

root=/home/workspace_data/works/myprojects
asset="$root/wise_combine_test-review-2-assets/omx"
common="$root/wise_combine_test-review-2-assets/common"
project="$root/wise_combine_test.omx"
mkdir -p "$asset/bin" "$asset/logs"

for mutant in 0 1 2 3 4 5 6; do
    cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
      -I"$common" -I"$project/include" -DMUTANT="$mutant" \
      "$asset/queue_harness.c" "$project/src/wct.c" \
      -o "$asset/bin/queue_harness_m$mutant"
done

result="$asset/queue-results.tsv"
printf 'mutant\tcase\trepetition\texit\toutput\n' > "$result"
for mutant in 0 1 2 3 4 5 6; do
    for case in 1 2 3 4 5 6; do
        model="$asset/models/Q$case.model"
        for repetition in 1 2; do
            set +e
            output=$("$asset/bin/queue_harness_m$mutant" "$model" 2>&1)
            exit_code=$?
            set -e
            printf '%s\tQ%s\t%s\t%s\t%s\n' "$mutant" "$case" "$repetition" "$exit_code" "$output" >> "$result"
        done
    done
done

awk -F '\t' 'NR > 1 { printf "M%s %s run%s exit=%s %s\n", $1,$2,$3,$4,$5 }' "$result" > "$asset/queue-results.txt"
