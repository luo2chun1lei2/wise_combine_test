#pragma once

#include <string>

namespace wise::replay {

struct Request {
  std::string report;
  std::string executable;
  std::string reports;
  std::string run_id;
};

struct Outcome {
  enum class Code { success, observed_mismatch, runtime_failure, invalid_report };
  Code code{Code::success};
};

Outcome run(const Request& request);

}
