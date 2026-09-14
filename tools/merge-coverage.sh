#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 BUILD_DIR FORK_DIR" >&2
    exit 2
fi

build=$1
forks=$2
command -v gcov-tool >/dev/null 2>&1 || {
    echo 'coverage failure: gcov-tool is required to merge fork profiles' >&2
    exit 1
}

profile_count=$(find "$forks" -type f -name '*.gcda' | wc -l)
[ "$profile_count" -gt 0 ] || exit 0

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
current=$work/parent
mkdir -p "$current"
for profile in "$build"/*.gcda; do
    [ -f "$profile" ] || continue
    name=${profile##*/}
    [ -f "$build/${name%.gcda}.gcno" ] || continue
    cp "$profile" "$current/"
done
next=$work/merge-a

for directory in "$forks"/*; do
    [ -d "$directory" ] || continue
    staged=$work/staged
    rm -rf "$staged"
    mkdir -p "$staged"
    find "$directory" -type f -name '*.gcda' | while IFS= read -r profile; do
        name=${profile##*/}
        [ -f "$build/${name%.gcda}.gcno" ] || continue
        cp "$profile" "$staged/$name"
    done
    find "$staged" -type f -name '*.gcda' | grep -q . || continue
    rm -rf "$next"
    gcov-tool merge -o "$next" "$current" "$staged" >/dev/null
    current=$next
    if [ "$next" = "$work/merge-a" ]; then next=$work/merge-b; else next=$work/merge-a; fi
done

find "$current" -type f -name '*.gcda' | while IFS= read -r profile; do
    name=${profile##*/}
    [ -f "$build/${name%.gcda}.gcno" ] || continue
    cp "$profile" "$build/$name"
done
