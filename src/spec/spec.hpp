#pragma once

#include "model/model.hpp"
#include <stdexcept>
#include <cstdint>
#include <string>
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

Document parse(const std::string& json);
std::string normalize(const std::string& json);

}  // namespace wise::spec
