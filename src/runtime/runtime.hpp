#pragma once

#include "generate/generate.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace wise::runtime {

enum class Status { passed, mismatch, protocol_error, timeout, crashed, launch_error };

struct StepResult {
  Status status{Status::launch_error};
  std::size_t index{0};
  std::string transition;
  std::string function;
  std::string observed_state;
  std::string stderr_text;
  int exit_status{-1};
  std::string detail;
};

struct RunResult {
  Status status{Status::passed};
  std::string flow_id;
  std::vector<StepResult> steps;
};

struct Options {
  std::string executable;
  std::vector<std::string> arguments;
  std::string working_directory;
  std::size_t step_timeout_ms{2000};
  std::size_t total_timeout_ms{30000};
  std::size_t output_limit{16U * 1024U * 1024U};
};

RunResult execute(const model::Model& model, const generate::Flow& flow,
                  const Options& options);

}  // namespace wise::runtime
