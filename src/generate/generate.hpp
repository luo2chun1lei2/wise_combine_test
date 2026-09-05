#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/model.hpp"

namespace wise::generate {

enum class GenerationStatus { dead_end, case_limit, step_limit };

struct Flow {
  std::string flow_id;
  std::vector<std::string> transition_ids;

  friend inline bool operator==(const Flow& lhs, const Flow& rhs) {
    return lhs.flow_id == rhs.flow_id && lhs.transition_ids == rhs.transition_ids;
  }
};

struct GenerationResult {
  GenerationStatus status{GenerationStatus::dead_end};
  std::vector<Flow> flows;

  friend inline bool operator==(const GenerationResult& lhs,
                                const GenerationResult& rhs) {
    return lhs.status == rhs.status && lhs.flows == rhs.flows;
  }
};

[[nodiscard]] GenerationResult generate(const model::Model& model, std::uint64_t seed);

}  // namespace wise::generate
