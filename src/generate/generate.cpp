#include "generate/generate.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <random>
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
  std::mt19937_64 rng;
  GenerationResult result;

  void visit(const std::string& state, std::vector<std::string>& sequence) {
    if (result.flows.size() >= model.limits().max_cases) {
      result.status = GenerationStatus::case_limit;
      return;
    }

    std::vector<const model::Transition*> choices;
    for (const auto* transition : transitions) {
      if (transition->from != state) {
        continue;
      }
      const bool violates_order = std::any_of(
          model.ordering_relations().begin(), model.ordering_relations().end(),
          [&](const auto& relation) {
            return relation.before == transition->id &&
                   std::find(sequence.begin(), sequence.end(), relation.after) != sequence.end();
          });
      if (violates_order) continue;
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
    std::shuffle(choices.begin(), choices.end(), rng);

    if (choices.empty()) {
      if (emitted.insert(sequence).second) {
        result.flows.push_back(Flow{sequence.empty() ? std::string{} : sequence.front(), sequence});
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
  model.validate();

  Search search{model, {}, {}, {}, std::mt19937_64(seed), GenerationResult{}};
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

void validate_flow(const model::Model& model, const Flow& flow) {
  model.validate();
  if (flow.transition_ids.empty()) {
    if (!flow.flow_id.empty()) throw std::invalid_argument("empty flow must have empty flow_id");
  } else if (flow.flow_id != flow.transition_ids.front()) {
    throw std::invalid_argument("flow_id must match first transition");
  }
  std::string state = model.initial_state();
  std::set<std::string> seen;
  for (const auto& id : flow.transition_ids) {
    const auto it = std::find_if(model.transitions().begin(), model.transitions().end(),
                                 [&](const auto& t) { return t.id == id; });
    if (it == model.transitions().end()) throw std::invalid_argument("flow references unknown transition: " + id);
    if (it->from != state) throw std::invalid_argument("flow transition is not reachable: " + id);
    for (const auto& relation : model.ordering_relations())
      if (relation.after == id && seen.count(relation.before) == 0U)
        throw std::invalid_argument("flow violates ordering relation: " + relation.before + " before " + id);
    state = it->to;
    seen.insert(id);
  }
  if (flow.transition_ids.size() > model.limits().max_steps)
    throw std::invalid_argument("flow exceeds max_steps");
}

}  // namespace wise::generate
