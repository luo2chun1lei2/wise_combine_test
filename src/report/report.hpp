#pragma once

#include "generate/generate.hpp"
#include "runtime/runtime.hpp"

#include <string>

namespace wise::report {

struct V2Metadata {
  std::string model_json;
  generate::GenerationStatus generation_status{generate::GenerationStatus::dead_end};
  generate::Flow flow;
  runtime::Options adapter;
  std::string adapter_sha256;
  runtime::RunResult result;
};

std::string json(const runtime::RunResult& result);
std::string text(const runtime::RunResult& result);
std::string payload_v2(const V2Metadata& metadata);
V2Metadata verify_payload_v2(const std::string& payload);
bool write(const runtime::RunResult& result, const std::string& directory, const std::string& run_id,
           const V2Metadata& metadata, std::string* error = nullptr);
bool write(const runtime::RunResult& result, const std::string& directory, const std::string& run_id,
           std::string* error = nullptr);

}
