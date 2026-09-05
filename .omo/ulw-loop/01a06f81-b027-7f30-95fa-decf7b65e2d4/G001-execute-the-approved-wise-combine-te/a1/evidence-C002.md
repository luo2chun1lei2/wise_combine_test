# C002 Runtime Failure Matrix

Current implementation commit: `ee6fca06462aef8486523e3d6699fbd724c5d783`

- Debug runtime matrix: 7/7 CTest cases passed.
- ASan/UBSan/LSan runtime matrix: 7/7 CTest cases passed with leak detection enabled.
- Covered modes: success, mismatch, malformed JSON, extra JSON, timeout, crash/non-zero exit, and output cap.
- Adapter constraints verified in task evidence: no-shell `fork`/`execve`, executable allowlist, bounded timeouts, process-group cleanup, and escaped JSON request/report fields.
- Source evidence: `.omo/evidence/wise-combine-test/task-5-runtime-verification.log` and `.omo/evidence/wise-combine-test/task-5-wise-combine-test.json`.
- Cleanup: no child processes or ports left by the verification runs; temporary build directories are under `/tmp`.
