#include "report/report.hpp"

#include "integrity/sha256.hpp"
#include "spec/spec.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace wise::report {
namespace {

using Json = spec::JsonValue;
using JsonObject = Json::Object;

const char* name(runtime::Status status) {
  switch (status) {
    case runtime::Status::passed: return "passed";
    case runtime::Status::mismatch: return "mismatch";
    case runtime::Status::protocol_error: return "protocol_error";
    case runtime::Status::adapter_error: return "adapter_error";
    case runtime::Status::timeout: return "timeout";
    case runtime::Status::crashed: return "crashed";
    case runtime::Status::launch_error: return "launch_error";
  }
  return "launch_error";
}

const char* name(generate::GenerationStatus status) {
  switch (status) {
    case generate::GenerationStatus::dead_end: return "dead_end";
    case generate::GenerationStatus::case_limit: return "case_limit";
    case generate::GenerationStatus::step_limit: return "step_limit";
  }
  return "dead_end";
}

std::string file_sha256(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot read adapter file " + path);
  return integrity::sha256_hex({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()});
}

std::string scalar_json(const model::Scalar& value) {
  if (value.kind == model::Scalar::Kind::string) return spec::encode_json_string(value.string_value);
  if (value.kind == model::Scalar::Kind::boolean) return value.boolean_value ? "true" : "false";
  if (value.kind == model::Scalar::Kind::integer) return std::to_string(value.integer_value);
  if (value.kind == model::Scalar::Kind::number) {
    std::ostringstream number;
    number.imbue(std::locale::classic());
    number << std::setprecision(std::numeric_limits<double>::max_digits10) << value.number_value;
    return number.str();
  }
  return "null";
}

std::string map_json(const std::map<std::string, model::Scalar>& values) {
  std::ostringstream out;
  out << '{';
  bool first = true;
  for (const auto& [key, value] : values) {
    if (!first) out << ',';
    first = false;
    out << spec::encode_json_string(key) << ':' << scalar_json(value);
  }
  out << '}';
  return out.str();
}

std::string result_json(const runtime::RunResult& result, bool include_returns) {
  std::ostringstream out;
  out << "{\"status\":\"" << name(result.status) << "\",\"flow_id\":"
      << spec::encode_json_string(result.flow_id) << ",\"steps\":[";
  for (std::size_t i = 0; i < result.steps.size(); ++i) {
    if (i != 0) out << ',';
    const auto& step = result.steps[i];
    out << "{\"index\":" << step.index
        << ",\"transition\":" << spec::encode_json_string(step.transition)
        << ",\"function\":" << spec::encode_json_string(step.function)
        << ",\"status\":\"" << name(step.status) << '"'
        << ",\"args\":" << map_json(step.arguments)
        << ",\"observed_state\":" << spec::encode_json_string(step.observed_state)
        << ",\"expected_state\":" << spec::encode_json_string(step.expected_state)
        << (include_returns ? std::string(",\"returns\":") + map_json(step.returns) : std::string())
        << ",\"stderr\":" << spec::encode_json_string(step.stderr_text)
        << ",\"exit_status\":" << step.exit_status
        << ",\"detail\":" << spec::encode_json_string(step.detail) << '}';
  }
  out << "]}";
  return out.str();
}

[[noreturn]] void invalid(const std::string& message) {
  throw std::runtime_error("invalid report v2 payload: " + message);
}

const Json& field(const JsonObject& object, const std::string& key) {
  const auto it = object.find(key);
  if (it == object.end()) invalid("missing " + key);
  return it->second;
}

void exact_keys(const JsonObject& object, std::initializer_list<const char*> keys) {
  for (const auto& [name, value] : object) {
    static_cast<void>(value);
    if (std::find_if(keys.begin(), keys.end(), [&](const char* key) { return name == key; }) == keys.end())
      invalid("unknown field " + name);
  }
}

std::string string_value(const Json& value) {
  if (const auto* text = std::get_if<std::string>(&value.data)) return *text;
  invalid("expected string");
}

std::size_t integer_value(const Json& value) {
  const auto* number = std::get_if<std::int64_t>(&value.data);
  if (number == nullptr || *number < 0) invalid("expected non-negative integer");
  return static_cast<std::size_t>(*number);
}

std::vector<std::string> string_array(const Json& value) {
  const auto* array = std::get_if<Json::Array>(&value.data);
  if (array == nullptr) invalid("expected string array");
  std::vector<std::string> result;
  result.reserve(array->size());
  for (const auto& item : *array) result.push_back(string_value(item));
  return result;
}

std::map<std::string, model::Scalar> scalar_map(const Json& value) {
  const auto* object = std::get_if<JsonObject>(&value.data);
  if (object == nullptr) invalid("expected scalar object");
  std::map<std::string, model::Scalar> result;
  for (const auto& [key, item] : *object) {
    model::Scalar scalar;
    if (std::holds_alternative<std::nullptr_t>(item.data)) scalar.kind = model::Scalar::Kind::null_value;
    else if (const auto* boolean = std::get_if<bool>(&item.data)) { scalar.kind = model::Scalar::Kind::boolean; scalar.boolean_value = *boolean; }
    else if (const auto* integer = std::get_if<std::int64_t>(&item.data)) { scalar.kind = model::Scalar::Kind::integer; scalar.integer_value = *integer; }
    else if (const auto* number = std::get_if<double>(&item.data)) { scalar.kind = model::Scalar::Kind::number; scalar.number_value = *number; }
    else if (const auto* text = std::get_if<std::string>(&item.data)) { scalar.kind = model::Scalar::Kind::string; scalar.string_value = *text; }
    else invalid("scalar object contains a non-scalar");
    result.emplace(key, scalar);
  }
  return result;
}

runtime::Status parse_status(const std::string& value) {
  if (value == "passed") return runtime::Status::passed;
  if (value == "mismatch") return runtime::Status::mismatch;
  if (value == "protocol_error") return runtime::Status::protocol_error;
  if (value == "adapter_error") return runtime::Status::adapter_error;
  if (value == "timeout") return runtime::Status::timeout;
  if (value == "crashed") return runtime::Status::crashed;
  if (value == "launch_error") return runtime::Status::launch_error;
  invalid("unknown status " + value);
}

const model::Transition* transition(const model::Model& model, const std::string& id) {
  for (const auto& item : model.transitions()) if (item.id == id) return &item;
  return nullptr;
}

bool scalar_equal(const model::Scalar& lhs, const model::Scalar& rhs) {
  if (lhs.kind != rhs.kind) return false;
  switch (lhs.kind) {
    case model::Scalar::Kind::null_value: return true;
    case model::Scalar::Kind::boolean: return lhs.boolean_value == rhs.boolean_value;
    case model::Scalar::Kind::integer: return lhs.integer_value == rhs.integer_value;
    case model::Scalar::Kind::number: return lhs.number_value == rhs.number_value;
    case model::Scalar::Kind::string: return lhs.string_value == rhs.string_value;
  }
  return false;
}

bool scalar_map_equal(const std::map<std::string, model::Scalar>& lhs,
                      const std::map<std::string, model::Scalar>& rhs) {
  if (lhs.size() != rhs.size()) return false;
  auto rhs_item = rhs.begin();
  for (const auto& [key, value] : lhs) {
    if (key != rhs_item->first || !scalar_equal(value, rhs_item->second)) return false;
    ++rhs_item;
  }
  return true;
}

void verify_result(const generate::Flow& flow, const spec::Document& document,
                   const runtime::RunResult& result) {
  if (result.flow_id != flow.flow_id) invalid("result flow_id does not match flow");
  if (result.steps.size() > flow.transition_ids.size()) invalid("result is longer than flow");
  if (result.status != runtime::Status::passed && result.steps.empty() &&
      result.status != runtime::Status::timeout)
    invalid("failure result cannot be empty");

  std::map<std::string, std::map<std::string, model::Scalar>> returned;
  for (std::size_t i = 0; i < result.steps.size(); ++i) {
    const auto& step = result.steps[i];
    if (step.index != i) invalid("result step index is not sequential");
    if (i != 0 && result.steps[i - 1].status != runtime::Status::passed)
      invalid("result contains steps after a failed step");

    const bool implicit_launch_failure = i == 0 && step.status == runtime::Status::launch_error &&
                                          step.transition.empty() && step.function.empty();
    const model::Transition* expected = nullptr;
    if (!implicit_launch_failure) {
      if (step.transition != flow.transition_ids[i]) invalid("result step does not match flow");
      expected = transition(document.model, step.transition);
      if (expected == nullptr) invalid("result references an unknown transition");
      if (step.function != expected->function) invalid("result function does not match model");
    }

    auto effective_arguments = expected == nullptr ? std::map<std::string, model::Scalar>{} : expected->args;
    if (expected != nullptr) {
      for (const auto& relation : document.model.argument_relations()) {
        if (relation.consumer_transition != expected->id) continue;
        const auto producer = returned.find(relation.producer_transition);
        if (producer == returned.end()) invalid("result argument producer is unavailable");
        const auto value = producer->second.find(relation.producer_output);
        if (value == producer->second.end()) invalid("result argument producer return is missing");
        effective_arguments[relation.consumer_argument] = value->second;
      }
      if (!scalar_map_equal(step.arguments, effective_arguments))
        invalid("result arguments do not match model bindings");
    }

    const std::string expected_state = expected == nullptr ? std::string{} :
        (expected->expect_present ? expected->expect : expected->to);
    if (step.expected_state != expected_state) invalid("result expected_state does not match model");
    if (step.status == runtime::Status::passed && !expected_state.empty() &&
        step.observed_state != expected_state) invalid("passed result has an unmatched observed state");

    if (step.status == runtime::Status::passed) returned[flow.transition_ids[i]] = step.returns;
  }

  if (result.status == runtime::Status::passed) {
    if (!flow.transition_ids.empty() && result.steps.size() != flow.transition_ids.size())
      invalid("passed result does not contain the complete flow");
    for (const auto& step : result.steps)
      if (step.status != runtime::Status::passed) invalid("passed result contains a failed step");
  } else if (!result.steps.empty()) {
    const auto& last = result.steps.back();
    if (last.status != result.status && result.status != runtime::Status::timeout)
      invalid("result status does not match its final step");
    if (last.status == runtime::Status::passed &&
        (result.status != runtime::Status::timeout || result.steps.size() == flow.transition_ids.size()))
      invalid("failure result ends with a passed final step");
  }
}

void write_one(const std::filesystem::path& path, const std::string& content) {
  std::ofstream output(path);
  if (!output) throw std::runtime_error("cannot write report file " + path.string());
  output << content;
  output.flush();
  if (!output) throw std::runtime_error("cannot finish writing report file " + path.string());
}

}  // namespace

std::string json(const runtime::RunResult& result) {
  return "{\"schema_version\":1," + result_json(result, false).substr(1);
}

std::string text(const runtime::RunResult& result) {
  std::ostringstream out;
  out << "flow " << result.flow_id << ": " << name(result.status) << '\n';
  for (const auto& step : result.steps)
    out << "step " << step.index << ' ' << step.function << ": " << name(step.status)
        << " state=" << step.observed_state << " exit=" << step.exit_status << ' ' << step.detail << '\n';
  return out.str();
}

std::string payload_v2(const V2Metadata& metadata) {
  static_cast<void>(spec::parse(metadata.model_json));
  std::ostringstream out;
  out << "{\"model\":" << metadata.model_json
      << ",\"generator\":{\"strategy\":\"seeded-dfs-v1\",\"termination_status\":\""
      << name(metadata.generation_status) << "\"}"
      << ",\"flow\":{\"flow_id\":" << spec::encode_json_string(metadata.flow.flow_id)
      << ",\"transition_ids\":[";
  for (std::size_t i = 0; i < metadata.flow.transition_ids.size(); ++i) {
    if (i != 0) out << ',';
    out << spec::encode_json_string(metadata.flow.transition_ids[i]);
  }
  out << "]},\"adapter\":{\"path\":" << spec::encode_json_string(metadata.adapter.executable)
      << ",\"sha256\":" << spec::encode_json_string(file_sha256(metadata.adapter.executable))
      << ",\"arguments\":[";
  for (std::size_t i = 0; i < metadata.adapter.arguments.size(); ++i) {
    if (i != 0) out << ',';
    out << spec::encode_json_string(metadata.adapter.arguments[i]);
  }
  out << "],\"working_directory\":" << spec::encode_json_string(metadata.adapter.working_directory)
      << "},\"runtime\":{\"step_timeout_ms\":" << metadata.adapter.step_timeout_ms
      << ",\"total_timeout_ms\":" << metadata.adapter.total_timeout_ms
      << ",\"output_limit_bytes\":" << metadata.adapter.output_limit
      << ",\"environment\":[\"PATH=/usr/bin:/bin\",\"LC_ALL=C\"]}"
      << ",\"result\":" << result_json(metadata.result, true) << '}';
  return out.str();
}

V2Metadata verify_payload_v2(const std::string& payload) {
  const auto root = spec::parse_json_value(payload);
  const auto* top = std::get_if<JsonObject>(&root.data);
  if (top == nullptr) invalid("expected object");
  exact_keys(*top, {"model", "generator", "flow", "adapter", "runtime", "result"});

  V2Metadata metadata;
  metadata.model_json = spec::canonical_json_value(field(*top, "model"));
  spec::Document document;
  try {
    document = spec::parse(metadata.model_json);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("invalid report v2 model: ") + error.what());
  }

  const auto& generator = std::get<JsonObject>(field(*top, "generator").data);
  exact_keys(generator, {"strategy", "termination_status"});
  if (string_value(field(generator, "strategy")) != "seeded-dfs-v1") invalid("unsupported generator strategy");
  const auto termination = string_value(field(generator, "termination_status"));
  if (termination == "dead_end") metadata.generation_status = generate::GenerationStatus::dead_end;
  else if (termination == "case_limit") metadata.generation_status = generate::GenerationStatus::case_limit;
  else if (termination == "step_limit") metadata.generation_status = generate::GenerationStatus::step_limit;
  else invalid("unknown generator termination_status");

  const auto& flow = std::get<JsonObject>(field(*top, "flow").data);
  exact_keys(flow, {"flow_id", "transition_ids"});
  metadata.flow.flow_id = string_value(field(flow, "flow_id"));
  metadata.flow.transition_ids = string_array(field(flow, "transition_ids"));
  try {
    generate::validate_flow(document.model, metadata.flow);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("invalid report v2 flow: ") + error.what());
  }

  const auto& adapter = std::get<JsonObject>(field(*top, "adapter").data);
  exact_keys(adapter, {"path", "sha256", "arguments", "working_directory"});
  metadata.adapter.executable = string_value(field(adapter, "path"));
  const auto digest = string_value(field(adapter, "sha256"));
  metadata.adapter.arguments = string_array(field(adapter, "arguments"));
  metadata.adapter.working_directory = string_value(field(adapter, "working_directory"));
  if (metadata.adapter.executable.empty() || !std::filesystem::path(metadata.adapter.executable).is_absolute())
    invalid("adapter path is not absolute");
  if (digest.size() != 64 || digest.find_first_not_of("0123456789abcdef") != std::string::npos)
    invalid("adapter sha256 is invalid");
  metadata.adapter_sha256 = digest;
  if (metadata.adapter.working_directory.empty() ||
      !std::filesystem::path(metadata.adapter.working_directory).is_absolute())
    invalid("adapter working_directory is not absolute");

  const auto& runtime_settings = std::get<JsonObject>(field(*top, "runtime").data);
  exact_keys(runtime_settings, {"step_timeout_ms", "total_timeout_ms", "output_limit_bytes", "environment"});
  metadata.adapter.step_timeout_ms = integer_value(field(runtime_settings, "step_timeout_ms"));
  metadata.adapter.total_timeout_ms = integer_value(field(runtime_settings, "total_timeout_ms"));
  metadata.adapter.output_limit = integer_value(field(runtime_settings, "output_limit_bytes"));
  const auto environment = string_array(field(runtime_settings, "environment"));
  const std::vector<std::string> fixed_environment{"PATH=/usr/bin:/bin", "LC_ALL=C"};
  if (metadata.adapter.step_timeout_ms == 0 || metadata.adapter.total_timeout_ms == 0 ||
      metadata.adapter.output_limit == 0 || environment != fixed_environment)
    invalid("runtime limits or environment do not match the fixed replay contract");

  const auto& result = std::get<JsonObject>(field(*top, "result").data);
  exact_keys(result, {"status", "flow_id", "steps"});
  metadata.result.status = parse_status(string_value(field(result, "status")));
  metadata.result.flow_id = string_value(field(result, "flow_id"));
  const auto* steps = std::get_if<Json::Array>(&field(result, "steps").data);
  if (steps == nullptr) invalid("result steps is not an array");
  for (const auto& step_value : *steps) {
    const auto& step = std::get<JsonObject>(step_value.data);
    exact_keys(step, {"index", "transition", "function", "status", "args", "observed_state",
                      "expected_state", "returns", "stderr", "exit_status", "detail"});
    runtime::StepResult parsed;
    parsed.index = integer_value(field(step, "index"));
    parsed.transition = string_value(field(step, "transition"));
    parsed.function = string_value(field(step, "function"));
    parsed.status = parse_status(string_value(field(step, "status")));
    parsed.arguments = scalar_map(field(step, "args"));
    parsed.observed_state = string_value(field(step, "observed_state"));
    parsed.expected_state = string_value(field(step, "expected_state"));
    parsed.returns = scalar_map(field(step, "returns"));
    parsed.stderr_text = string_value(field(step, "stderr"));
    const auto* exit_status = std::get_if<std::int64_t>(&field(step, "exit_status").data);
    if (exit_status == nullptr || *exit_status < -1 || *exit_status > 255) invalid("invalid exit_status");
    parsed.exit_status = static_cast<int>(*exit_status);
    parsed.detail = string_value(field(step, "detail"));
    metadata.result.steps.push_back(std::move(parsed));
  }
  verify_result(metadata.flow, document, metadata.result);
  return metadata;
}

bool write(const runtime::RunResult& result, const std::string& directory, const std::string& run_id,
           const V2Metadata& metadata, std::string* error) {
  try {
    std::filesystem::create_directories(directory);
    const std::filesystem::path base = std::filesystem::path(directory) / run_id;
    write_one(base.string() + ".json", json(result));
    write_one(base.string() + ".txt", text(result));
    write_one(base.string() + ".v2.json", integrity::wrap_v2(payload_v2(metadata)));
    return true;
  } catch (const std::exception& caught) {
    if (error != nullptr) *error = caught.what();
    return false;
  }
}

bool write(const runtime::RunResult& result, const std::string& directory, const std::string& run_id,
           std::string* error) {
  try {
    std::filesystem::create_directories(directory);
    const std::filesystem::path base = std::filesystem::path(directory) / run_id;
    write_one(base.string() + ".json", json(result));
    write_one(base.string() + ".txt", text(result));
    return true;
  } catch (const std::exception& caught) {
    if (error != nullptr) *error = caught.what();
    return false;
  }
}

}
