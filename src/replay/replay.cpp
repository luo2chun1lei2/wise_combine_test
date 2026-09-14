#include "replay/replay.hpp"

#include "integrity/sha256.hpp"
#include "report/report.hpp"
#include "runtime/runtime.hpp"
#include "spec/spec.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace wise::replay {
namespace {

std::string read_file(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot read " + path);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool same_file(const std::filesystem::path& left, const std::filesystem::path& right,
               std::error_code& error) {
  if (!std::filesystem::exists(left, error) || !std::filesystem::exists(right, error)) {
    error.clear();
    return false;
  }
  return std::filesystem::equivalent(left, right, error);
}

}  // namespace

Outcome run(const Request& request) {
  Outcome outcome;
  try {
    std::string payload;
    if (!integrity::verify_v2(read_file(request.report), &payload))
      throw std::runtime_error("report integrity verification failed");
    auto metadata = report::verify_payload_v2(payload);

    std::error_code path_error;
    const auto input_report = std::filesystem::weakly_canonical(request.report, path_error);
    if (path_error) throw std::runtime_error("cannot resolve input report: " + path_error.message());
    const auto input_directory = input_report.parent_path();
    const auto output_directory = std::filesystem::weakly_canonical(request.reports, path_error);
    if (path_error) throw std::runtime_error("cannot resolve output directory: " + path_error.message());

    bool collision = same_file(input_directory, output_directory, path_error);
    path_error.clear();
    const auto planned_report = std::filesystem::weakly_canonical(
        std::filesystem::path(request.reports) / (request.run_id + "-0.v2.json"), path_error);
    if (!path_error && same_file(planned_report, input_report, path_error)) collision = true;
    if (path_error) throw std::runtime_error("cannot resolve output report: " + path_error.message());
    if (collision) {
      std::cerr << "replay output directory would overwrite the input report\n";
      outcome.code = Outcome::Code::runtime_failure;
      return outcome;
    }

    if (!std::filesystem::is_directory(metadata.adapter.working_directory)) {
      std::cerr << "recorded working directory does not exist: "
                << metadata.adapter.working_directory << '\n';
      outcome.code = Outcome::Code::runtime_failure;
      return outcome;
    }
    if (!runtime::validate_executable(request.executable)) {
      std::cerr << "explicit adapter is not on the allowlist\n";
      outcome.code = Outcome::Code::runtime_failure;
      return outcome;
    }
    if (integrity::sha256_hex(read_file(request.executable)) != metadata.adapter_sha256) {
      std::cerr << "explicit adapter digest does not match the recorded adapter digest\n";
      outcome.code = Outcome::Code::runtime_failure;
      return outcome;
    }

    const auto resolved_executable =
        std::filesystem::weakly_canonical(request.executable, path_error);
    if (path_error) throw std::runtime_error("cannot resolve adapter: " + path_error.message());
    metadata.adapter.executable = resolved_executable.string();
    const auto document = spec::parse(metadata.model_json);
    const auto result = runtime::execute(document.model, metadata.flow, metadata.adapter);
    metadata.result = result;

    std::string report_error;
    if (!report::write(result, request.reports, request.run_id + "-0", metadata, &report_error)) {
      std::cerr << "unable to write reports in '" << request.reports << "': "
                << report_error << '\n';
      outcome.code = Outcome::Code::runtime_failure;
      return outcome;
    }
    if (result.status == runtime::Status::passed) return outcome;
    if (result.status == runtime::Status::mismatch)
      outcome.code = Outcome::Code::observed_mismatch;
    else
      outcome.code = Outcome::Code::runtime_failure;
    return outcome;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    outcome.code = Outcome::Code::invalid_report;
    return outcome;
  }
}

}
