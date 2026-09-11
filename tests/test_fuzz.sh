#!/bin/sh
set -eu

bin=./bin/wise-combine-test
tmp=${TMPDIR:-/tmp}/wct-fuzz.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

# Deterministic malformed and boundary corpus.  The parser may accept or reject
# a case, but it must never time out, terminate by signal, or return a usage
# error for a supplied model.
printf '' > "$tmp/empty.model"
printf 'schema 999\n' > "$tmp/schema.model"
printf 'schema 1\nunknown value\n' > "$tmp/unknown.model"
printf 'schema 1\nstate_graph g s\nstate s\nstate s\n' > "$tmp/duplicate.model"
awk 'BEGIN { printf "schema 1\n#"; for (i=0;i<8192;i++) printf "x"; printf "\n" }' \
    > "$tmp/long.model"
awk 'BEGIN { srand(11); printf "schema 1\n"; for (i=0;i<128;i++) {
    n=1+int(rand()*40); for(j=0;j<n;j++) printf "%c", 33+int(rand()*90); printf "\n"
} }' > "$tmp/random.model"

for model in "$tmp"/*.model; do
    set +e
    timeout 2 "$bin" --model "$model" --mode state >/dev/null 2>&1
    rc=$?
    set -e
    case $rc in
        0|1) ;;
        *) echo "fuzz failure: $model returned $rc" >&2; exit 1 ;;
    esac
done

echo 'DSL boundary fuzz tests passed'
