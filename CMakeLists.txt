cmake_minimum_required(VERSION 3.16)

project(wise_combine_test VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

# The smoke test is part of the documented project baseline.  Do not let an
# existing cache with BUILD_TESTING=OFF silently remove it from CTest.
set(BUILD_TESTING ON CACHE BOOL "Build and register the project tests" FORCE)
enable_testing()

option(WISE_COMBINE_ENABLE_SANITIZERS
       "Enable AddressSanitizer, UndefinedBehaviorSanitizer, and LeakSanitizer"
       OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wise-combine-compile-options INTERFACE)
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

add_executable(wise-combine-smoke src/smoke.cpp)
target_link_libraries(wise-combine-smoke PRIVATE wise-combine-compile-options)

add_executable(wise-combine src/main.cpp src/cli/cli.cpp)
target_include_directories(wise-combine PRIVATE src)
target_link_libraries(wise-combine PRIVATE wise-combine-report wise-combine-runtime
  wise-combine-generate wise-combine-spec wise-combine-model
  wise-combine-compile-options)

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
add_test(NAME runtime_mismatch COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> mismatch)
add_test(NAME runtime_malformed COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> malformed)
add_test(NAME runtime_extra_json COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> extra)
add_test(NAME runtime_timeout COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> timeout 100)
add_test(NAME runtime_crash COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> crash)
add_test(NAME runtime_output_cap COMMAND wise-combine-runtime-test $<TARGET_FILE:wise-combine-runtime-fixture> cap)
set_tests_properties(runtime_timeout PROPERTIES TIMEOUT 5)

add_test(NAME cli_validate COMMAND wise-combine validate
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/state_valid.json)
add_test(NAME cli_generate COMMAND wise-combine generate
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/relation_valid.json)
add_test(NAME cli_run COMMAND wise-combine run
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/relation_valid.json
  --adapter ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_ok
  --reports ${CMAKE_CURRENT_BINARY_DIR}/cli-reports --run-id integration)
add_test(NAME cli_run_failure COMMAND wise-combine run
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/relation_valid.json
  --adapter ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/bin/adapter_mismatch
  --reports ${CMAKE_CURRENT_BINARY_DIR}/cli-failure-reports --run-id integration-failure)
set_tests_properties(cli_run_failure PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_invalid COMMAND wise-combine validate
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/invalid.json)
set_tests_properties(cli_invalid PROPERTIES WILL_FAIL TRUE)
