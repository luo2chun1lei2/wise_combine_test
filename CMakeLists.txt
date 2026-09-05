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

set(CMAKE_CXX_STANDARD 17)
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
