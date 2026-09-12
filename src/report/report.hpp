#pragma once
#include "runtime/runtime.hpp"
#include <string>
namespace wise::report {
std::string json(const runtime::RunResult& result);
std::string text(const runtime::RunResult& result);
bool write(const runtime::RunResult& result, const std::string& directory, const std::string& run_id,
           std::string* error = nullptr);
}
