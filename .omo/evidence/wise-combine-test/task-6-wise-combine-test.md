# Todo 6 verification

Implementation adds the `wise-combine` Linux CLI, integration fixtures,
measurement fields, reports, sanitizer instructions, and README documentation.

## Automated checks

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
(cd build && ctest --output-on-failure)
100% tests passed, 0 tests failed out of 18

cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DWISE_COMBINE_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
(cd build-asan && ctest --output-on-failure)
100% tests passed, 0 tests failed out of 18
```

## Manual CLI checks

```text
validate exit=0
generate exit=0
run with adapter_ok exit=0
run with adapter_mismatch exit=4
report exit=0
```

The successful run produced a two-step relation flow and `*-summary.json`
with `case_count`, `passed`, `failed`, `wall_time_ns`, `cpu_time_ns`, and
`peak_rss_bytes`. The failing run produced JSON and text reports identifying
the flow and failed step.

Valgrind and coverage are documented as optional commands in `README.md`.
