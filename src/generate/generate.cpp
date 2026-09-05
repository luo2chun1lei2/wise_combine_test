#include "generate/generate.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace wise::generate {
namespace {

struct Search {
  const model::Model& model;
  std::vector<const model::Transition*> transitions;
  std::map<std::string, std::vector<std::string>> prerequisites;
  std::set<std::vector<std::string>> emitted;
  GenerationResult result;

  void visit(const std::string& state, std::vector<std::string>& sequence) {
    if (result.flows.size() >= model.limits().max_cases) {
      result.status = GenerationStatus::case_limit;
      return;
    }

    std::vector<const model::Transition*> choices;
    for (const auto* transition : transitions) {
      if (transition->from != state ||
          (std::find(sequence.begin(), sequence.end(), transition->id) != sequence.end() &&
           transition->from != transition->to)) {
        continue;
      }
      const auto required = prerequisites.find(transition->id);
      if (required != prerequisites.end() &&
          !std::all_of(required->second.begin(), required->second.end(), [&](const auto& id) {
            return std::find(sequence.begin(), sequence.end(), id) != sequence.end();
          })) {
        continue;
      }
      choices.push_back(transition);
    }
    std::sort(choices.begin(), choices.end(), [](const auto* lhs, const auto* rhs) {
      return lhs->id < rhs->id;
    });

    if (choices.empty()) {
      if (!sequence.empty() && emitted.insert(sequence).second) {
        result.flows.push_back(Flow{sequence.front(), sequence});
      }
      return;
    }
    if (sequence.size() >= model.limits().max_steps) {
      if (!sequence.empty() && emitted.insert(sequence).second) {
        result.flows.push_back(Flow{sequence.front(), sequence});
      }
      result.status = GenerationStatus::step_limit;
      return;
    }

    for (const auto* transition : choices) {
      if (result.flows.size() >= model.limits().max_cases) {
        result.status = GenerationStatus::case_limit;
        return;
      }
      sequence.push_back(transition->id);
      visit(transition->to, sequence);
      sequence.pop_back();
      if (result.status == GenerationStatus::case_limit) return;
    }
  }
};

}  // namespace

GenerationResult generate(const model::Model& model, std::uint64_t seed) {
  static_cast<void>(seed);
  model.validate();

  Search search{model, {}, {}, {}, GenerationResult{}};
  for (const auto& transition : model.transitions()) {
    search.transitions.push_back(&transition);
  }
  for (const auto& relation : model.argument_relations()) {
    search.prerequisites[relation.consumer_transition].push_back(
        relation.producer_transition);
  }
  for (const auto& relation : model.ordering_relations()) {
    search.prerequisites[relation.after].push_back(relation.before);
  }
  if (model.limits().max_cases == 0U) {
    search.result.status = GenerationStatus::case_limit;
    return search.result;
  }
  std::vector<std::string> sequence;
  search.visit(model.initial_state(), sequence);
  if (search.result.status == GenerationStatus::dead_end &&
      search.result.flows.size() >= model.limits().max_cases) {
    search.result.status = GenerationStatus::case_limit;
  }
  return search.result;
}

}  // namespace wise::generate
