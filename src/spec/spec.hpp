#pragma once

#include "model/model.hpp"
#include <stdexcept>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace wise::spec {

struct Diagnostic {
  std::string pointer;
  std::string message;
};

class SpecError final : public std::runtime_error {
 public:
  explicit SpecError(std::vector<Diagnostic> diagnostics);
  [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
 private:
  std::vector<Diagnostic> diagnostics_;
};

struct Document {
  int version{1};
  model::Model model;
  std::uint64_t seed{0};
  std::string canonical_json;
};

struct AdapterResponse {
  int protocol{0};
  std::string status;
  std::string observed_state;
  std::map<std::string, model::Scalar> returns;
  std::string stderr_output;
};

struct JsonValue {
  using Object = std::map<std::string, JsonValue>;
  using Array = std::vector<JsonValue>;
  std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Object, Array> data;
};

Document parse(const std::string& json);
std::string normalize(const std::string& json);
JsonValue parse_json_value(const std::string& json);
std::string canonical_json_value(const JsonValue& value);
AdapterResponse parse_adapter_response(const std::string& json);
void validate_report(const std::string& json);
struct IntegrityEnvelope { std::string payload; std::string digest; };
IntegrityEnvelope parse_integrity_envelope(const std::string& json);
std::string encode_json_string(const std::string& value);
void validate_json(const std::string& json);

}  // namespace wise::spec
