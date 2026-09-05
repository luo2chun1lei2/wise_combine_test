#include "generate/generate.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using wise::generate::GenerationStatus;
using wise::generate::generate;
using wise::model::Function;
using wise::model::Model;
using wise::model::ModelError;
using wise::model::OrderingRelation;
using wise::model::State;
using wise::model::Transition;

namespace {

Model base_model() {
  Model model;
  model.add_state(State{"start"});
  model.add_state(State{"end"});
  model.add_function(Function{"f", {}, {}});
  model.set_initial_state("start");
  model.set_limits({8, 8, 1});
  return model;
}

void linear_flow_is_terminal_and_deterministic() {
  Model model = base_model();
  model.add_transition(Transition{"finish", "start", "end", "f"});
  const auto first = generate(model, 11);
  const auto second = generate(model, 11);
  if (first.status != GenerationStatus::dead_end || first.flows.size() != 1U ||
      first.flows.front().transition_ids != std::vector<std::string>{"finish"} ||
      first.flows.front().flow_id != "finish" || first.flows != second.flows) {
    throw std::runtime_error("linear fixture mismatch");
  }
}

void branch_respects_case_limit() {
  Model model = base_model();
  model.add_transition(Transition{"z-branch", "start", "end", "f"});
  model.add_transition(Transition{"a-branch", "start", "end", "f"});
  model.set_limits({2, 8, 1});
  const auto result = generate(model, 7);
  if (result.status != GenerationStatus::case_limit || result.flows.size() != 2U ||
      result.flows[0].flow_id != "a-branch" || result.flows[1].flow_id != "z-branch") {
    throw std::runtime_error("branch fixture mismatch");
  }
}

void self_loop_is_one_bounded_flow() {
  Model model;
  model.add_state(State{"start"});
  model.add_function(Function{"tick", {}, {}});
  model.set_initial_state("start");
  model.set_limits({4, 3, 1});
  model.add_transition(Transition{"tick", "start", "start", "tick"});
  const auto result = generate(model, 3);
  if (result.status != GenerationStatus::step_limit || result.flows.size() != 1U ||
      result.flows.front().transition_ids !=
          std::vector<std::string>{"tick", "tick", "tick"}) {
    throw std::runtime_error("self-loop fixture mismatch");
  }
}

void ordering_cycle_is_rejected() {
  Model model = base_model();
  model.add_transition(Transition{"a", "start", "end", "f"});
  model.add_transition(Transition{"b", "start", "end", "f"});
  model.add_ordering_relation(OrderingRelation{"a", "b"});
  model.add_ordering_relation(OrderingRelation{"b", "a"});
  try {
    static_cast<void>(generate(model, 0));
  } catch (const ModelError& error) {
    if (error.code() == ModelError::Code::contradictory_ordering) return;
    throw std::runtime_error("wrong ordering error");
  }
  throw std::runtime_error("ordering cycle accepted");
}

void zero_cases_is_explicit() {
  Model model = base_model();
  model.add_transition(Transition{"finish", "start", "end", "f"});
  model.set_limits({0, 8, 1});
  const auto result = generate(model, 0);
  if (result.status != GenerationStatus::case_limit || !result.flows.empty()) {
    throw std::runtime_error("zero case limit mismatch");
  }
}

}  // namespace

int main() {
  try {
    linear_flow_is_terminal_and_deterministic();
    branch_respects_case_limit();
    self_loop_is_one_bounded_flow();
    ordering_cycle_is_rejected();
    zero_cases_is_explicit();
  } catch (const std::exception& error) {
    std::cerr << "generator test failure: " << error.what() << '\n';
    return 1;
  }
  std::cout << "generator: all tests passed\n";
}
