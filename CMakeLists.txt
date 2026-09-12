cmake_minimum_required(VERSION 3.16)

project(wise_combine_test VERSION 0.1.0 LANGUAGES CXX)
find_package(OpenSSL REQUIRED)

include(CTest)

# The smoke test is part of the documented project baseline.  Do not let an
# existing cache with BUILD_TESTING=OFF silently remove it from CTest.
set(BUILD_TESTING ON CACHE BOOL "Build and register the project tests" FORCE)
enable_testing()

option(WISE_COMBINE_ENABLE_SANITIZERS
       "Enable AddressSanitizer, UndefinedBehaviorSanitizer, and LeakSanitizer"
       OFF)
option(WISE_COMBINE_ENABLE_COVERAGE "Enable compiler coverage instrumentation" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wise-combine-compile-options INTERFACE)
add_library(wise-combine-integrity src/integrity/sha256.cpp)
target_include_directories(wise-combine-integrity PUBLIC src)
target_link_libraries(wise-combine-integrity PUBLIC OpenSSL::Crypto wise-combine-spec wise-combine-compile-options)
add_executable(wise-combine-sha256-test tests/sha256_test.cpp)
target_link_libraries(wise-combine-sha256-test PRIVATE wise-combine-integrity)
add_test(NAME sha256 COMMAND wise-combine-sha256-test)
add_executable(wise-combine-integrity-v2-test tests/integrity_v2_test.cpp)
target_link_libraries(wise-combine-integrity-v2-test PRIVATE wise-combine-integrity)
add_test(NAME integrity_v2 COMMAND wise-combine-integrity-v2-test)
if(MSVC)
  target_compile_options(wise-combine-compile-options INTERFACE /W4)
else()
  target_compile_options(wise-combine-compile-options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
  )
endif()

if(WISE_COMBINE_ENABLE_SANITIZERS)
  if(MSVC)
    message(FATAL_ERROR "WISE_COMBINE_ENABLE_SANITIZERS requires GCC or Clang")
  endif()
  target_compile_options(wise-combine-compile-options INTERFACE
    -fsanitize=address,undefined,leak
    -fno-omit-frame-pointer
  )
  target_link_options(wise-combine-compile-options INTERFACE
    -fsanitize=address,undefined,leak
  )
endif()
if(WISE_COMBINE_ENABLE_COVERAGE)
  if(MSVC)
    message(FATAL_ERROR "WISE_COMBINE_ENABLE_COVERAGE requires GCC or Clang")
  endif()
  target_compile_options(wise-combine-compile-options INTERFACE --coverage -fno-inline)
  target_link_options(wise-combine-compile-options INTERFACE --coverage)
  add_custom_target(coverage-check
    COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/coverage_gate.cmake)
endif()

add_executable(wise-combine-smoke src/smoke.cpp)
target_link_libraries(wise-combine-smoke PRIVATE wise-combine-compile-options)

add_executable(wise-combine src/main.cpp src/cli/cli.cpp)
target_include_directories(wise-combine PRIVATE src)
target_link_libraries(wise-combine PRIVATE wise-combine-report wise-combine-runtime
  wise-combine-generate wise-combine-spec wise-combine-model
  wise-combine-integrity wise-combine-compile-options)

add_executable(wise-combine-smoke-test tests/smoke_test.cpp)
target_link_libraries(wise-combine-smoke-test PRIVATE wise-combine-compile-options)
add_test(NAME smoke COMMAND wise-combine-smoke-test)

add_library(wise-combine-model src/model/model.cpp)
target_include_directories(wise-combine-model PUBLIC src)
target_link_libraries(wise-combine-model PRIVATE wise-combine-compile-options)

add_executable(wise-combine-model-test tests/model_test.cpp)
target_link_libraries(wise-combine-model-test PRIVATE wise-combine-model
  wise-combine-compile-options)
add_test(NAME model COMMAND wise-combine-model-test)

add_library(wise-combine-spec src/spec/spec.cpp)
target_include_directories(wise-combine-spec PUBLIC src)
target_link_libraries(wise-combine-spec PUBLIC wise-combine-model
  wise-combine-compile-options)
add_executable(wise-combine-spec-test tests/spec_test.cpp)
target_link_libraries(wise-combine-spec-test PRIVATE wise-combine-spec)
add_test(NAME spec_valid COMMAND wise-combine-spec-test)
add_test(NAME spec_invalid COMMAND wise-combine-spec-test)

add_library(wise-combine-generate src/generate/generate.cpp)
target_include_directories(wise-combine-generate PUBLIC src)
target_link_libraries(wise-combine-generate PUBLIC wise-combine-spec
  wise-combine-compile-options)
add_executable(wise-combine-generator-test tests/generator_test.cpp)
target_link_libraries(wise-combine-generator-test PRIVATE wise-combine-generate)
add_test(NAME generator_determinism COMMAND wise-combine-generator-test)
add_test(NAME generator_limits COMMAND wise-combine-generator-test)

add_library(wise-combine-runtime src/runtime/runtime.cpp)
target_include_directories(wise-combine-runtime PUBLIC src)
target_link_libraries(wise-combine-runtime PUBLIC wise-combine-generate
  wise-combine-compile-options)
add_library(wise-combine-report src/report/report.cpp)
target_include_directories(wise-combine-report PUBLIC src)
target_link_libraries(wise-combine-report PUBLIC wise-combine-runtime
  wise-combine-compile-options)
add_executable(wise-combine-runtime-fixture tests/fixture_adapter.cpp)
target_link_libraries(wise-combine-runtime-fixture PRIVATE wise-combine-compile-options)
add_executable(wise-combine-runtime-test tests/runtime_test.cpp)
target_link_libraries(wise-combine-runtime-test PRIVATE wise-combine-report
  wise-combine-runtime wise-combine-generate wise-combine-compile-options)
add_test(NAME runtime_pass COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture>)
add_test(NAME runtime_allowlist COMMAND wise-combine-runtime-test /bin/true allowlist)
add_test(NAME runtime_mismatch COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> mismatch)
add_test(NAME runtime_malformed COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> malformed)
add_test(NAME runtime_extra_json COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> extra)
add_test(NAME runtime_unknown_status COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> unknown-status)
add_test(NAME runtime_duplicate_status COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> duplicate-status)
add_test(NAME runtime_formatted COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> formatted)
add_test(NAME runtime_unicode COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> unicode)
add_test(NAME runtime_escaped COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> escaped)
add_test(NAME runtime_adapter_error COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> error)
add_test(NAME runtime_timeout COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> timeout 100)
add_test(NAME runtime_crash COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> crash)
add_test(NAME runtime_output_cap COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> cap)
add_test(NAME runtime_argument_relation COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> relation)
set_tests_properties(runtime_timeout PROPERTIES TIMEOUT 5)

add_test(NAME cli_validate COMMAND wise-combine validate
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/state_workflow.json)
add_test(NAME cli_generate COMMAND wise-combine generate
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/relation_workflow.json)
add_test(NAME cli_run COMMAND wise-combine run
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/relation_workflow.json
  --adapter ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_ok
  --reports ${CMAKE_CURRENT_BINARY_DIR}/cli-reports --run-id integration)
add_test(NAME cli_run_failure COMMAND wise-combine run
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/relation_workflow.json
  --adapter ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_mismatch
  --reports ${CMAKE_CURRENT_BINARY_DIR}/cli-failure-reports --run-id integration-failure)
set_tests_properties(cli_run_failure PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_invalid COMMAND wise-combine validate
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/invalid.json)
set_tests_properties(cli_invalid PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_report_write_failure COMMAND ${CMAKE_COMMAND}
  -DCLI=$<TARGET_FILE:wise-combine>
  -DSPEC=${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/relation_workflow.json
  -DADAPTER=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_ok
  -DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/report-write-faults
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/report_write_failure.cmake)
add_test(NAME cli_reports_error COMMAND bash
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_reports_error_test.sh
  $<TARGET_FILE:wise-combine>
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/relation_workflow.json
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_ok)
add_test(NAME cli_run_id_error COMMAND bash
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_run_id_error_test.sh
  $<TARGET_FILE:wise-combine>
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/relation_workflow.json
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_ok)
add_test(NAME cli_verify_report COMMAND bash
  -c "$<TARGET_FILE:wise-combine> run ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/relation_workflow.json --adapter ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_ok --reports ${CMAKE_CURRENT_BINARY_DIR}/verify-reports --run-id verify >/dev/null && $<TARGET_FILE:wise-combine> verify-report ${CMAKE_CURRENT_BINARY_DIR}/verify-reports/verify-0.json")
add_test(NAME cli_verify_report_invalid COMMAND ${CMAKE_COMMAND}
  -DCLI=$<TARGET_FILE:wise-combine>
  -DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/invalid-report
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_report_invalid.cmake)
add_test(NAME cli_verify_summary COMMAND ${CMAKE_COMMAND}
  -DCLI=$<TARGET_FILE:wise-combine>
  -DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/verify-summary
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_summary.cmake)
add_test(NAME cli_hash_report COMMAND ${CMAKE_COMMAND}
  -DCLI=$<TARGET_FILE:wise-combine>
  -DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/hash-report
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_hash_report_test.cmake)
add_test(NAME cli_verify_report_v2 COMMAND ${CMAKE_COMMAND}
  -DCLI=$<TARGET_FILE:wise-combine>
  -DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/verify-report-v2
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_verify_report_v2.cmake)
add_test(NAME cli_wrap_report_v2 COMMAND ${CMAKE_COMMAND}
  -DCLI=$<TARGET_FILE:wise-combine>
  -DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/wrap-report-v2
  -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_wrap_report_v2.cmake)
foreach(evaluation_mutant RANGE 0 6)
  add_executable(adapter_eval_${evaluation_mutant} tests/evaluation/queue_adapter.cpp)
  target_compile_definitions(adapter_eval_${evaluation_mutant} PRIVATE MUTANT=${evaluation_mutant})
  target_link_libraries(adapter_eval_${evaluation_mutant} PRIVATE wise-combine-compile-options)
  set_target_properties(adapter_eval_${evaluation_mutant} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/fixtures/bin")
endforeach()
foreach(evaluation_scenario RANGE 1 6)
  add_test(NAME evaluation_q${evaluation_scenario} COMMAND ${CMAKE_COMMAND}
    -DCLI=$<TARGET_FILE:wise-combine>
    -DCLEAN_ADAPTER=$<TARGET_FILE:adapter_eval_0>
    -DMUTANT_ADAPTER=$<TARGET_FILE:adapter_eval_${evaluation_scenario}>
    -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DCASE_DIR=${CMAKE_CURRENT_BINARY_DIR}/evaluation-q${evaluation_scenario}
    -DSCENARIO=${evaluation_scenario}
    -DMUTANT=${evaluation_scenario}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/evaluation/run_case.cmake)
  set_tests_properties(evaluation_q${evaluation_scenario} PROPERTIES TIMEOUT 30)
endforeach()
