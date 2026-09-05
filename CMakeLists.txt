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
