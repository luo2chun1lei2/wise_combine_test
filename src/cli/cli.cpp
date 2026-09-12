#include "cli/cli.hpp"

#include "generate/generate.hpp"
#include "report/report.hpp"
#include "runtime/runtime.hpp"
#include "spec/spec.hpp"
#include "integrity/sha256.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <time.h>
#include <vector>

namespace wise::cli {
namespace {

constexpr int kParseError = 2;
constexpr int kGenerationExhausted = 3;
constexpr int kObservedFailure = 4;
constexpr int kRuntimeFailure = 5;
constexpr int kUsageError = 6;

struct Measurement {
  std::uint64_t wall_time_ns{0};
  std::uint64_t cpu_time_ns{0};
  std::uint64_t peak_rss_bytes{0};
};

std::uint64_t clock_ns(clockid_t clock_id) {
  timespec value{};
  if (clock_gettime(clock_id, &value) != 0) return 0;
  return static_cast<std::uint64_t>(value.tv_sec) * 1000000000ULL +
         static_cast<std::uint64_t>(value.tv_nsec);
}

Measurement measure(std::uint64_t wall_start, std::uint64_t cpu_start) {
  struct rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  const auto wall_end = clock_ns(CLOCK_MONOTONIC);
  const auto cpu_end = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
  const auto rss = usage.ru_maxrss < 0 ? 0ULL : static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
  return {wall_end >= wall_start ? wall_end - wall_start : 0,
          cpu_end >= cpu_start ? cpu_end - cpu_start : 0, rss};
}

std::string read_file(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot read " + path);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string escape(const std::string& value) {
  std::ostringstream out;
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20U) out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c) << std::dec;
        else out << static_cast<char>(c);
    }
  }
  return out.str();
}

const char* generation_status(generate::GenerationStatus status) {
  switch (status) {
    case generate::GenerationStatus::dead_end: return "dead_end";
    case generate::GenerationStatus::case_limit: return "case_limit";
    case generate::GenerationStatus::step_limit: return "step_limit";
  }
  return "unknown";
}

void usage(std::ostream& out) {
  out << "usage: wise-combine <validate|generate|run|report|verify-report|hash-report> ...\n"
      << "  validate SPEC\n"
      << "  generate SPEC\n"
      << "  run SPEC --adapter EXEC [--arg ARG]... [--reports DIR] [--run-id ID]\n"
      << "  report REPORT.json|REPORT.txt\n"
      << "  verify-report REPORT.json\n"
      << "  hash-report REPORT [EXPECTED_SHA256]\n";
}

int parse_spec(const std::string& path, spec::Document& document) {
  try {
    document = spec::parse(read_file(path));
    return 0;
  } catch (const spec::SpecError& error) {
    std::cerr << "spec error: " << error.what() << '\n';
    for (const auto& diagnostic : error.diagnostics()) {
      std::cerr << diagnostic.pointer << ": " << diagnostic.message << '\n';
    }
    return kParseError;
  } catch (const std::exception& error) {
    std::cerr << "input error: " << error.what() << '\n';
    return kParseError;
  }
}

std::string generation_json(const spec::Document& document,
                            const generate::GenerationResult& result,
                            const Measurement& measurement) {
  std::ostringstream out;
  out << "{\"status\":\"" << generation_status(result.status) << "\",\"case_count\":"
      << result.flows.size() << ",\"flows\":[";
  for (std::size_t i = 0; i < result.flows.size(); ++i) {
    if (i != 0) out << ',';
    out << "{\"flow_id\":\"" << escape(result.flows[i].flow_id) << "\",\"transitions\":[";
    for (std::size_t j = 0; j < result.flows[i].transition_ids.size(); ++j) {
      if (j != 0) out << ',';
      out << '"' << escape(result.flows[i].transition_ids[j]) << '"';
    }
    out << "]}";
  }
  out << "],\"seed\":" << document.seed << ",\"wall_time_ns\":" << measurement.wall_time_ns
      << ",\"cpu_time_ns\":" << measurement.cpu_time_ns
      << ",\"peak_rss_bytes\":" << measurement.peak_rss_bytes << "}\n";
  return out.str();
}

int command_validate(const std::string& path) {
  const auto wall = clock_ns(CLOCK_MONOTONIC);
  const auto cpu = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
  spec::Document document;
  const int status = parse_spec(path, document);
  if (status != 0) return status;
  const auto measurement = measure(wall, cpu);
  std::cout << "{\"valid\":true,\"version\":" << document.version
            << ",\"state_count\":" << document.model.states().size()
            << ",\"transition_count\":" << document.model.transitions().size()
            << ",\"wall_time_ns\":" << measurement.wall_time_ns
            << ",\"cpu_time_ns\":" << measurement.cpu_time_ns
            << ",\"peak_rss_bytes\":" << measurement.peak_rss_bytes << "}\n";
  return 0;
}

int command_generate(const std::string& path) {
  const auto wall = clock_ns(CLOCK_MONOTONIC);
  const auto cpu = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
  spec::Document document;
  const int status = parse_spec(path, document);
  if (status != 0) return status;
  const auto result = generate::generate(document.model, document.seed);
  std::cout << generation_json(document, result, measure(wall, cpu));
  return result.status == generate::GenerationStatus::dead_end ? 0 : kGenerationExhausted;
}

int command_run(const std::vector<std::string>& args) {
  if (args.size() < 3U || args[1] != "--adapter") return kUsageError;
  const auto wall = clock_ns(CLOCK_MONOTONIC);
  const auto cpu = clock_ns(CLOCK_PROCESS_CPUTIME_ID);
  spec::Document document;
  const int parse_status = parse_spec(args[0], document);
  if (parse_status != 0) return parse_status;
  runtime::Options options;
  options.executable = args[2];
  std::string reports = "reports";
  std::string run_id = "run";
  for (std::size_t i = 3; i < args.size(); ++i) {
    if (args[i] == "--arg" && i + 1U < args.size()) options.arguments.push_back(args[++i]);
    else if (args[i] == "--reports" && i + 1U < args.size()) reports = args[++i];
    else if (args[i] == "--run-id" && i + 1U < args.size()) run_id = args[++i];
    else return kUsageError;
  }
  if (run_id.empty() || run_id == "." || run_id == ".." ||
      run_id.find_first_of("/\\") != std::string::npos) {
    std::cerr << "run-id must be a single file-name component\n";
    return kUsageError;
  }
  const auto generated = generate::generate(document.model, document.seed);
  std::error_code report_directory_error;
  std::filesystem::create_directories(reports, report_directory_error);
  if (report_directory_error) {
    std::cerr << "unable to create reports directory '" << reports
              << "': " << report_directory_error.message() << '\n';
    return kRuntimeFailure;
  }
  std::size_t passed = 0;
  std::size_t failed = 0;
  int exit_status = generated.status == generate::GenerationStatus::dead_end ? 0 : kGenerationExhausted;
  std::size_t index = 0;
  for (const auto& flow : generated.flows) {
    const auto result = runtime::execute(document.model, flow, options);
    std::string report_error;
    if (!wise::report::write(result, reports, run_id + "-" + std::to_string(index), &report_error)) {
      std::cerr << "unable to write reports in '" << reports << "': " << report_error << '\n';
      return kRuntimeFailure;
    }
    if (result.status == runtime::Status::passed) ++passed;
    else { ++failed; exit_status = result.status == runtime::Status::mismatch ? kObservedFailure : kRuntimeFailure; }
    ++index;
  }
  const auto measurement = measure(wall, cpu);
  std::ofstream summary(std::filesystem::path(reports) / (run_id + "-summary.json"));
  if (!summary) {
    std::cerr << "unable to write reports summary in '" << reports << "'\n";
    return kRuntimeFailure;
  }
  summary << "{\"schema_version\":1,\"generation_status\":\"" << generation_status(generated.status)
          << "\",\"case_count\":" << generated.flows.size() << ",\"passed\":" << passed
          << ",\"failed\":" << failed << ",\"wall_time_ns\":" << measurement.wall_time_ns
          << ",\"cpu_time_ns\":" << measurement.cpu_time_ns
          << ",\"peak_rss_bytes\":" << measurement.peak_rss_bytes
          << ",\"seed\":" << document.seed << "}\n";
  summary.flush();
  if (!summary) {
    std::cerr << "unable to finish writing reports summary in '" << reports << "'\n";
    return kRuntimeFailure;
  }
  std::cout << "{\"schema_version\":1,\"generation_status\":\"" << generation_status(generated.status)
            << "\",\"case_count\":" << generated.flows.size() << ",\"passed\":" << passed
            << ",\"failed\":" << failed << ",\"wall_time_ns\":" << measurement.wall_time_ns
            << ",\"cpu_time_ns\":" << measurement.cpu_time_ns
            << ",\"peak_rss_bytes\":" << measurement.peak_rss_bytes
            << ",\"seed\":" << document.seed << "}\n";
  return exit_status;
}

int command_report(const std::string& path) {
  try { std::cout << read_file(path); return 0; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return kParseError; }
}

int command_verify_report(const std::string& path) {
  try {
    const auto content = read_file(path);
    spec::validate_report(content);
    std::cout << "{\"valid\":true,\"schema_version\":1}\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return kParseError; }
}

int command_hash_report(const std::string& path, const std::string& expected = {}) {
  try { const auto digest = wise::integrity::sha256_hex(read_file(path)); std::cout << digest << '\n'; return expected.empty() || digest == expected ? 0 : kObservedFailure; }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return kParseError; }
}

}  // namespace

int run(int argc, char** argv) {
  if (argc < 3) { usage(std::cerr); return kUsageError; }
  const std::string command = argv[1];
  if (command == "validate" && argc == 3) return command_validate(argv[2]);
  if (command == "generate" && argc == 3) return command_generate(argv[2]);
  if (command == "report" && argc == 3) return command_report(argv[2]);
  if (command == "verify-report" && argc == 3) return command_verify_report(argv[2]);
  if (command == "hash-report" && (argc == 3 || argc == 4)) return command_hash_report(argv[2], argc == 4 ? argv[3] : "");
  if (command == "run") {
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
    const int status = command_run(args);
    if (status == kUsageError) usage(std::cerr);
    return status;
  }
  usage(std::cerr);
  return kUsageError;
}

}  // namespace wise::cli
