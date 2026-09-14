#!/bin/sh
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
bin_dir=${WCT_QUEUE_BIN_DIR:-$root/build/queue-oracle}
results=${WCT_QUEUE_RESULTS:-$bin_dir/results.tsv}
cc=${CC:-cc}
cflags=${WCT_QUEUE_CFLAGS:--std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -I"$root/include"}
ldflags=${WCT_QUEUE_LDFLAGS:-}
runner=${WCT_QUEUE_RUNNER:-}

mkdir -p "$bin_dir"
printf 'mutant\tcase\trepetition\texit\toutput\n' > "$results"

# Reuse the canonical product object so coverage data lands in
# build/wct.gcda and stays compatible with profiles already emitted.
mkdir -p "$root/build"
if [ ! -f "$root/build/wct.o" ]; then
    "$cc" $cflags -c "$root/src/wct.c" -o "$root/build/wct.o"
fi

for mutant in 0 1 2 3 4 5 6; do
    # shellcheck disable=SC2086
    "$cc" $cflags -DMUTANT="$mutant" \
        "$root/tests/queue_harness.c" "$root/build/wct.o" \
        $ldflags -o "$bin_dir/queue_harness_m$mutant"
done

run_case() {
    mutant=$1
    case_label=$2
    repetition=$3
    model=$4
    harness="$bin_dir/queue_harness_m$mutant"
    profile_prefix="$bin_dir/gcov/m$mutant"
    set +e
    if [ -n "$runner" ]; then
        if [ -n "${WCT_COVERAGE_ROOT:-}" ]; then
            output=$(env GCOV_PREFIX="$profile_prefix" GCOV_PREFIX_STRIP=0 \
                $runner "$harness" "$root/$model" 2>&1)
        else
            output=$($runner "$harness" "$root/$model" 2>&1)
        fi
    else
        if [ -n "${WCT_COVERAGE_ROOT:-}" ]; then
            output=$(env GCOV_PREFIX="$profile_prefix" GCOV_PREFIX_STRIP=0 \
                "$harness" "$root/$model" 2>&1)
        else
            output=$("$harness" "$root/$model" 2>&1)
        fi
    fi
    exit_code=$?
    set -e
    printf '%s\t%s\t%s\t%s\t%s\n' "$mutant" "$case_label" "$repetition" "$exit_code" "$output" >> "$results"
}

for case_id in 1 2 3 4 5 6; do
    for repetition in 1 2; do
        for mutant in 0 1 2 3 4 5 6; do
            run_case "$mutant" "Q$case_id" "$repetition" "fixtures/queue-oracle/Q${case_id}.model"
        done
    done
done

for probe in cycle repeat subset; do
    set +e
    output=$("$bin_dir/queue_harness_m0" "$root/fixtures/queue-oracle/probe-$probe.model" 2>&1)
    exit_code=$?
    set -e
    printf 'probe\t%s\t1\t%s\t%s\n' "$probe" "$exit_code" "$output" >> "$results"
done

if [ -n "${WCT_COVERAGE_ROOT:-}" ]; then
    tools/merge-coverage.sh "$root/build" "$bin_dir/gcov" >/dev/null
fi

for mutant in 0 1 2 3 4 5 6; do
    for case_id in 1 2 3 4 5 6; do
        for repetition in 1 2; do
            row=$(awk -F '\t' -v m="$mutant" -v c="Q$case_id" -v r="$repetition" \
                '$1==m && $2==c && $3==r {print; exit}' "$results")
            [ -n "$row" ]
            exit_code=$(printf '%s' "$row" | cut -f4)
            output=$(printf '%s' "$row" | cut -f5)
            case $exit_code in
                0|1) ;;
                *) echo "queue oracle: unexpected exit $exit_code for M$mutant Q$case_id run$repetition" >&2; exit 1 ;;
            esac
            if [ "$mutant" -eq 0 ]; then
                [ "$exit_code" -eq 0 ]
                printf '%s' "$output" | grep -q '^rc=0 steps=[1-9][0-9]* failures=0 uncovered=0 '
            else
                case $mutant in
                    1) expected_case=Q1 ;;
                    2) expected_case=Q2 ;;
                    3) expected_case=Q3 ;;
                    4) expected_case=Q4 ;;
                    5) expected_case=Q5 ;;
                    6) expected_case=Q6 ;;
                    *) continue ;;
                esac
                [ "$case_id" = "$expected_case" ] || continue
                [ "$exit_code" -eq 1 ]
                printf '%s' "$output" | grep -q '^rc=-1 .* failures=1 uncovered=1 error=call result assertion failed$'
            fi
        done
    done
done

for mutant in 1 2 3 4 5 6; do
    failed_rows=$(awk -F '\t' -v m="$mutant" '$1==m && $4==1 {n++} END {print n+0}' "$results")
    [ "$failed_rows" -ge 2 ] || {
        echo "queue oracle: M$mutant expected at least one case detected twice, got $failed_rows failures" >&2
        exit 1
    }
    # Lock the exact documented defect-to-case mapping.
    case $mutant in
        1) expected=Q1 ;;
        2) expected=Q2 ;;
        3) expected=Q3 ;;
        4) expected=Q4 ;;
        5) expected=Q5 ;;
        6) expected=Q6 ;;
    esac
    [ "$(awk -F '\t' -v m="$mutant" -v c="$expected" '$1==m && $2==c && $4==1 {n++} END {print n+0}' "$results")" -eq 2 ]
done

[ "$(awk -F '\t' '$1=="probe" && $2=="cycle" {print $4}' "$results")" = 1 ]
[ "$(awk -F '\t' '$1=="probe" && $2=="subset" {print $4}' "$results")" = 0 ]
[ "$(awk -F '\t' '$1=="probe" && $2=="repeat" {print $4}' "$results")" = 0 ]
for case_id in 1 2 3 4 5 6; do
    for mutant in 0 1 2 3 4 5 6; do
        [ "$(awk -F '\t' -v m="$mutant" -v c="Q$case_id" '$1==m && $2==c && $3==1 {print $5}' "$results")" \
          = "$(awk -F '\t' -v m="$mutant" -v c="Q$case_id" '$1==m && $2==c && $3==2 {print $5}' "$results")" ]
    done
done

printf 'Queue oracle regression passed: 84 matrix runs, 12 clean passes, 6/6 matching mutants detected, 3 generation probes\n'
