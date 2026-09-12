# Additional probes

All commands were run from `wise_combine_test.lazycodex` after the Debug build.

## Seed

The same `tests/fixtures/relation_valid.json` was copied twice, changing only
`seed` from 9 to 10. The generated transition list was identical:

```text
seed=9:  status=dead_end case_count=1 flow=produce,consume
seed=10: status=dead_end case_count=1 flow=produce,consume
```

Only timing and the printed seed differed. This is consistent with
`src/generate/generate.cpp:76`, where the seed is discarded.

## Case limit

Changing `max_cases` to zero produced:

```text
{"status":"case_limit","case_count":0,"flows":[],"seed":9,...}
exit=3
```

## Report path failure

```text
tmp=/tmp/lazy-report-file
: > "$tmp"
./build/wise-combine run tests/fixtures/state_valid.json \
  --adapter tests/fixtures/bin/adapter_ok --reports "$tmp" --run-id fail
```

The command terminated with `std::filesystem::filesystem_error` (“cannot
create directories: Not a directory”) and exit 134. No stable CLI error or
structured report was produced.

## Native test and sanitizer evidence

```text
ctest --test-dir build --output-on-failure       19/19 passed
ctest --test-dir build-asan --output-on-failure 19/19 passed
```

Running `ctest` from the source directory is not the documented invocation;
it reports zero tests because CTest uses the current directory's test file.
