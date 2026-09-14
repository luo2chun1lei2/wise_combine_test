if(NOT DEFINED CLI OR NOT DEFINED SPEC OR NOT DEFINED ADAPTER OR NOT DEFINED CASE_DIR)
  message(FATAL_ERROR "missing test configuration")
endif()

get_filename_component(REPLAY_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(RELATIVE_ADAPTER "tests/fixtures/bin/adapter_ok")

file(REMOVE_RECURSE "${CASE_DIR}")
file(MAKE_DIRECTORY "${CASE_DIR}")
set(REPORTS "${CASE_DIR}/reports")
set(REPLAY_REPORTS "${CASE_DIR}/replay-reports")
set(V2_REPORT "${REPORTS}/clean-0.v2.json")
set(MARKER "${CASE_DIR}/adapter-invoked")

execute_process(
  COMMAND "${CLI}" run "${SPEC}" --adapter "${ADAPTER}"
          --reports "${REPORTS}" --run-id clean
  WORKING_DIRECTORY "${REPLAY_SOURCE_DIR}"
  RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error
)
if(NOT run_result EQUAL 0 OR NOT run_error STREQUAL "")
  message(FATAL_ERROR "clean run failed: ${run_result}: ${run_output} ${run_error}")
endif()
foreach(name clean-0.json clean-0.txt clean-0.v2.json)
  if(NOT EXISTS "${REPORTS}/${name}")
    message(FATAL_ERROR "clean run did not create ${name}")
  endif()
endforeach()
function(expect_verify report expected_status expected_output)
  execute_process(
    COMMAND "${CLI}" verify-report-v2 "${report}"
    RESULT_VARIABLE actual_status OUTPUT_VARIABLE actual_output ERROR_VARIABLE actual_error
  )
  if(NOT actual_status EQUAL expected_status OR NOT actual_output STREQUAL expected_output)
    message(FATAL_ERROR "verify ${report}: expected ${expected_status} '${expected_output}', got ${actual_status} '${actual_output}' ${actual_error}")
  endif()
endfunction()

expect_verify("${V2_REPORT}" 0
  "{\"valid\":true,\"integrity_verified\":true,\"payload_valid\":true,\"schema_version\":2}\n")

# Envelope digest tampering is rejected before payload validation.
file(READ "${V2_REPORT}" clean_envelope)
string(REGEX REPLACE "\"digest\":\"[0-9a-f]+\""
  "\"digest\":\"0000000000000000000000000000000000000000000000000000000000000000\""
  tampered_envelope "${clean_envelope}")
file(WRITE "${CASE_DIR}/tampered-clean-envelope.json" "${tampered_envelope}")
expect_verify("${CASE_DIR}/tampered-clean-envelope.json" 2 "")

# The explicit replay adapter may be relative to the command's working directory.
# Replay must persist the resolved absolute path so the report contract holds.
execute_process(
  COMMAND "${CLI}" replay "${V2_REPORT}" --adapter "${RELATIVE_ADAPTER}"
          --reports "${REPLAY_REPORTS}" --run-id replay
  WORKING_DIRECTORY "${REPLAY_SOURCE_DIR}"
  RESULT_VARIABLE replay_result
  OUTPUT_VARIABLE replay_output
  ERROR_VARIABLE replay_error
)
if(NOT replay_result EQUAL 0 OR NOT replay_error STREQUAL "")
  message(FATAL_ERROR "clean replay failed: ${replay_result}: ${replay_output} ${replay_error}")
endif()
foreach(name replay-0.json replay-0.txt replay-0.v2.json)
  if(NOT EXISTS "${REPLAY_REPORTS}/${name}")
    message(FATAL_ERROR "replay did not create ${name}")
  endif()
endforeach()
expect_verify("${REPLAY_REPORTS}/replay-0.v2.json" 0
  "{\"valid\":true,\"integrity_verified\":true,\"payload_valid\":true,\"schema_version\":2}\n")

# Payload schema keys are exact and types are checked after envelope integrity.
file(WRITE "${CASE_DIR}/extra-payload.json" "{\"model\":{\"version\":1},\"generator\":{},\"flow\":{},\"adapter\":{},\"runtime\":{},\"result\":{},\"extra\":1}")
execute_process(COMMAND "${CLI}" wrap-report-v2 "${CASE_DIR}/extra-payload.json" OUTPUT_VARIABLE envelope ERROR_VARIABLE wrap_error)
file(WRITE "${CASE_DIR}/extra-envelope.json" "${envelope}")
expect_verify("${CASE_DIR}/extra-envelope.json" 2 "")

file(WRITE "${CASE_DIR}/missing-payload.json" "{\"model\":{\"version\":1},\"flow\":{},\"adapter\":{},\"runtime\":{},\"result\":{}}")
execute_process(COMMAND "${CLI}" wrap-report-v2 "${CASE_DIR}/missing-payload.json" OUTPUT_VARIABLE envelope)
file(WRITE "${CASE_DIR}/missing-envelope.json" "${envelope}")
expect_verify("${CASE_DIR}/missing-envelope.json" 2 "")

file(WRITE "${CASE_DIR}/wrong-type-payload.json" "{\"model\":\"x\",\"generator\":{},\"flow\":{},\"adapter\":{},\"runtime\":{},\"result\":{}}")
execute_process(COMMAND "${CLI}" wrap-report-v2 "${CASE_DIR}/wrong-type-payload.json" OUTPUT_VARIABLE envelope)
file(WRITE "${CASE_DIR}/wrong-type-envelope.json" "${envelope}")
expect_verify("${CASE_DIR}/wrong-type-envelope.json" 2 "")

# A byte change without changing the recorded digest is rejected.
set(tampered "${envelope}")
string(REGEX REPLACE "\"digest\":\"[0-9a-f]+\"" "\"digest\":\"deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef\"" tampered "${tampered}")
file(WRITE "${CASE_DIR}/tampered-envelope.json" "${tampered}")
expect_verify("${CASE_DIR}/tampered-envelope.json" 2 "")

# Build a schema-valid flow that contradicts the parsed model's initial state.
file(READ "${SPEC}" model_json)
file(SHA256 "${ADAPTER}" adapter_digest)
set(adapter_json "{\"path\":\"${ADAPTER}\",\"sha256\":\"${adapter_digest}\",\"arguments\":[],\"working_directory\":\"${CASE_DIR}\"}")
set(runtime_json "{\"step_timeout_ms\":2000,\"total_timeout_ms\":30000,\"output_limit_bytes\":16777216,\"environment\":[\"PATH=/usr/bin:/bin\",\"LC_ALL=C\"]}")
set(contradiction "{\"model\":${model_json},\"generator\":{\"strategy\":\"seeded-dfs-v1\",\"termination_status\":\"dead_end\"},\"flow\":{\"flow_id\":\"consume\",\"transition_ids\":[\"consume\"]},\"adapter\":${adapter_json},\"runtime\":${runtime_json},\"result\":{\"status\":\"passed\",\"flow_id\":\"consume\",\"steps\":[]}}")
file(WRITE "${CASE_DIR}/contradiction-payload.json" "${contradiction}")
execute_process(COMMAND "${CLI}" wrap-report-v2 "${CASE_DIR}/contradiction-payload.json" OUTPUT_VARIABLE envelope)
file(WRITE "${CASE_DIR}/contradiction-envelope.json" "${envelope}")
expect_verify("${CASE_DIR}/contradiction-envelope.json" 2 "")

# Runtime can return a zero-step timeout, but step-derived failures cannot be empty.
set(empty_mismatch "{\"model\":${model_json},\"generator\":{\"strategy\":\"seeded-dfs-v1\",\"termination_status\":\"dead_end\"},\"flow\":{\"flow_id\":\"produce\",\"transition_ids\":[\"produce\",\"consume\"]},\"adapter\":${adapter_json},\"runtime\":${runtime_json},\"result\":{\"status\":\"mismatch\",\"flow_id\":\"produce\",\"steps\":[]}}")
file(WRITE "${CASE_DIR}/empty-mismatch-payload.json" "${empty_mismatch}")
execute_process(COMMAND "${CLI}" wrap-report-v2 "${CASE_DIR}/empty-mismatch-payload.json" OUTPUT_VARIABLE envelope)
file(WRITE "${CASE_DIR}/empty-mismatch-envelope.json" "${envelope}")
expect_verify("${CASE_DIR}/empty-mismatch-envelope.json" 2 "")

# A different allowlisted adapter digest is rejected before a child can start.
set(nonexistent_output "${CASE_DIR}/digest-mismatch-reports")
set(MARKER_REPORTS "${CASE_DIR}/marker-reports")
execute_process(
  COMMAND "${CLI}" run "${SPEC}" --adapter "${MARKER_ADAPTER}" --arg "${MARKER}"
          --reports "${MARKER_REPORTS}" --run-id marker
  RESULT_VARIABLE marker_run OUTPUT_VARIABLE marker_run_output ERROR_VARIABLE marker_run_error
)
if(NOT marker_run EQUAL 0 OR NOT marker_run_error STREQUAL "")
  message(FATAL_ERROR "marker run failed: ${marker_run}: ${marker_run_output} ${marker_run_error}")
endif()
file(REMOVE "${MARKER}")
execute_process(
  COMMAND "${CLI}" replay "${MARKER_REPORTS}/marker-0.v2.json"
          --adapter "${DIGEST_MISMATCH_ADAPTER}"
          --reports "${nonexistent_output}" --run-id clean
  RESULT_VARIABLE mismatch_result OUTPUT_VARIABLE mismatch_output ERROR_VARIABLE mismatch_error
)
if(NOT mismatch_result EQUAL 5 OR NOT mismatch_error MATCHES "digest does not match")
  message(FATAL_ERROR "digest mismatch replay accepted: ${mismatch_result}: ${mismatch_output} ${mismatch_error}")
endif()
if(EXISTS "${MARKER}")
  message(FATAL_ERROR "digest-mismatch replay started a child process")
endif()
if(EXISTS "${nonexistent_output}")
  message(FATAL_ERROR "digest-mismatch replay created output reports")
endif()

# A recorded working directory must exist before any child process starts.
set(missing_wd_payload "{\"model\":${model_json},\"generator\":{\"strategy\":\"seeded-dfs-v1\",\"termination_status\":\"dead_end\"},\"flow\":{\"flow_id\":\"produce\",\"transition_ids\":[\"produce\",\"consume\"]},\"adapter\":{\"path\":\"${ADAPTER}\",\"sha256\":\"${adapter_digest}\",\"arguments\":[],\"working_directory\":\"${CASE_DIR}/missing-working-directory\"},\"runtime\":${runtime_json},\"result\":{\"status\":\"timeout\",\"flow_id\":\"produce\",\"steps\":[]}}")
file(WRITE "${CASE_DIR}/missing-wd-payload.json" "${missing_wd_payload}")
execute_process(COMMAND "${CLI}" wrap-report-v2 "${CASE_DIR}/missing-wd-payload.json" OUTPUT_VARIABLE envelope)
file(WRITE "${CASE_DIR}/missing-wd-envelope.json" "${envelope}")
expect_verify("${CASE_DIR}/missing-wd-envelope.json" 0
  "{\"valid\":true,\"integrity_verified\":true,\"payload_valid\":true,\"schema_version\":2}\n")
execute_process(
  COMMAND "${CLI}" replay "${CASE_DIR}/missing-wd-envelope.json" --adapter "${ADAPTER}"
          --reports "${CASE_DIR}/missing-wd-reports" --run-id replay
  RESULT_VARIABLE missing_result OUTPUT_VARIABLE missing_output ERROR_VARIABLE missing_error
)
if(NOT missing_result EQUAL 5 OR NOT missing_error MATCHES "working directory does not exist")
  message(FATAL_ERROR "missing working directory accepted: ${missing_result}: ${missing_output} ${missing_error}")
endif()

# Output may not replace the input envelope or reports.
set(ALIAS_OUTPUT "${CASE_DIR}/alias-output")
file(MAKE_DIRECTORY "${ALIAS_OUTPUT}")
file(SHA256 "${V2_REPORT}" input_before_alias)
file(CREATE_LINK "${V2_REPORT}" "${ALIAS_OUTPUT}/alias-0.json" SYMBOLIC)
execute_process(
  COMMAND "${CLI}" replay "${V2_REPORT}" --adapter "${ADAPTER}"
          --reports "${ALIAS_OUTPUT}" --run-id alias
  RESULT_VARIABLE alias_result OUTPUT_VARIABLE alias_output ERROR_VARIABLE alias_error
)
file(SHA256 "${V2_REPORT}" input_after_alias)
if(NOT alias_result EQUAL 5 OR NOT alias_error MATCHES "output report path already exists")
  message(FATAL_ERROR "output alias accepted: ${alias_result}: ${alias_output} ${alias_error}")
endif()
if(NOT input_after_alias STREQUAL input_before_alias)
  message(FATAL_ERROR "output alias changed the input report")
endif()
if(EXISTS "${ALIAS_OUTPUT}/alias-0.v2.json")
  message(FATAL_ERROR "output alias replay created a v2 report")
endif()

execute_process(
  COMMAND "${CLI}" replay "${V2_REPORT}" --adapter "${ADAPTER}"
          --reports "${REPORTS}" --run-id clean
  RESULT_VARIABLE collision_result OUTPUT_VARIABLE collision_output ERROR_VARIABLE collision_error
)
if(NOT collision_result EQUAL 5 OR NOT collision_error MATCHES "overwrite the input")
  message(FATAL_ERROR "output collision accepted: ${collision_result}: ${collision_output} ${collision_error}")
endif()

# Historical mismatch payloads validate and replay with the observed-mismatch code.
set(MISMATCH_REPORTS "${CASE_DIR}/mismatch-reports")
execute_process(
  COMMAND "${CLI}" run "${SPEC}" --adapter "${MISMATCH_ADAPTER}" --reports "${MISMATCH_REPORTS}" --run-id mismatch
  RESULT_VARIABLE mismatch_run OUTPUT_VARIABLE mismatch_run_output ERROR_VARIABLE mismatch_run_error
)
if(NOT mismatch_run EQUAL 4)
  message(FATAL_ERROR "mismatch run failed unexpectedly: ${mismatch_run}: ${mismatch_run_output} ${mismatch_run_error}")
endif()
expect_verify("${MISMATCH_REPORTS}/mismatch-0.v2.json" 0
  "{\"valid\":true,\"integrity_verified\":true,\"payload_valid\":true,\"schema_version\":2}\n")
execute_process(
  COMMAND "${CLI}" replay "${MISMATCH_REPORTS}/mismatch-0.v2.json" --adapter "${MISMATCH_ADAPTER}"
          --reports "${CASE_DIR}/mismatch-replay" --run-id replay
  RESULT_VARIABLE mismatch_replay OUTPUT_VARIABLE mismatch_replay_output ERROR_VARIABLE mismatch_replay_error
)
if(NOT mismatch_replay EQUAL 4 OR NOT EXISTS "${CASE_DIR}/mismatch-replay/replay-0.v2.json")
  message(FATAL_ERROR "mismatch replay failed: ${mismatch_replay}: ${mismatch_replay_output} ${mismatch_replay_error}")
endif()
