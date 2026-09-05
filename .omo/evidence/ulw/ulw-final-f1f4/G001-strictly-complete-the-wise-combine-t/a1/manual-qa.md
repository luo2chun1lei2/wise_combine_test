# Final Manual QA

Reviewed commit: 48951f251dad3beea1dbac5cca5d10f7fd02ec7a
Tree: 46714a6a9a46ec6d0176183283dfa03b79ac069e

Surface: Linux CLI auxiliary surface plus generated JSON/TXT reports.

PASS matrix:
- validate state workflow: exit 0
- generate relation workflow: exit 0
- run relation workflow with adapter_ok: exit 0, passed=1
- run relation workflow with adapter_mismatch: exit 4
- malformed spec: exit 2
- timeout/crash runtime fixtures: assertions pass
- report generated TXT: exit 0
- summaries include case_count, passed, failed, wall_time_ns, cpu_time_ns, peak_rss_bytes
- final Debug and ASan/UBSan/LSan CTest: 19/19 each

Cleanup: no adapter/runtime process, port, or QA temp directory remains. F1-F4 artifact paths are non-empty and committed.
