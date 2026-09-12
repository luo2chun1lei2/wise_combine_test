#include "generate/generate.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using wise::generate::GenerationStatus;
using wise::generate::Flow;
using wise::generate::generate;
using wise::generate::validate_flow;
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
      result.flows[0].flow_id == result.flows[1].flow_id ||
      (result.flows[0].flow_id != "a-branch" && result.flows[0].flow_id != "z-branch") ||
      (result.flows[1].flow_id != "a-branch" && result.flows[1].flow_id != "z-branch")) {
    throw std::runtime_error("branch fixture mismatch");
  }
  if (generate(model, 7).flows != result.flows)
    throw std::runtime_error("seeded branches are not reproducible");
  bool varied = false;
  for (std::uint64_t seed = 0; seed < 32; ++seed)
    varied = varied || generate(model, seed).flows != result.flows;
  if (!varied) throw std::runtime_error("seed did not affect choices");
  Model reversed = base_model();
  reversed.add_transition(Transition{"a-branch", "start", "end", "f"});
  reversed.add_transition(Transition{"z-branch", "start", "end", "f"});
  reversed.set_limits({2, 8, 1});
  if (generate(reversed, 7).flows != result.flows)
    throw std::runtime_error("declaration order affected seeded flows");
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

void non_self_cycle_can_repeat_within_step_limit() {
  Model model;
  model.add_state(State{"a"});
  model.add_state(State{"b"});
  model.add_function(Function{"tick", {}, {}});
  model.set_initial_state("a");
  model.set_limits({2, 3, 1});
  model.add_transition(Transition{"forward", "a", "b", "tick"});
  model.add_transition(Transition{"back", "b", "a", "tick"});
  const auto result = generate(model, 1);
  if (result.flows.size() != 1U ||
      result.status != GenerationStatus::step_limit ||
      result.flows.front().transition_ids !=
          std::vector<std::string>{"forward", "back", "forward"}) {
    throw std::runtime_error("non-self cycle was not bounded and repeated");
  }
  model.add_ordering_relation(OrderingRelation{"forward", "back"});
  const auto ordered = generate(model, 1);
  if (ordered.status != GenerationStatus::dead_end || ordered.flows.size() != 1U ||
      ordered.flows.front().transition_ids != std::vector<std::string>{"forward", "back"})
    throw std::runtime_error("repeat violated global before relation");
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

void empty_model_emits_empty_flow() {
  Model model = base_model();
  model.set_limits({2, 8, 1});
  const auto result = generate(model, 0);
  if (result.status != GenerationStatus::dead_end || result.flows.size() != 1U ||
      !result.flows.front().flow_id.empty() || !result.flows.front().transition_ids.empty()) {
    throw std::runtime_error("empty model did not emit an empty flow: status=" + std::to_string(static_cast<int>(result.status)) + " size=" + std::to_string(result.flows.size()));
  }
  model.set_limits({1, 8, 1});
  const auto bounded = generate(model, 0);
  if (bounded.status != GenerationStatus::case_limit || bounded.flows != result.flows)
    throw std::runtime_error("empty flow ignored case limit");
  model.set_limits({0, 8, 1});
  if (!generate(model, 0).flows.empty())
    throw std::runtime_error("zero case budget emitted empty flow");
}

void persisted_flow_validation_rejects_invalid_sequences() {
  Model model = base_model();
  model.add_transition(Transition{"finish", "start", "end", "f"});
  validate_flow(model, generate(model, 0).flows.front());
  const auto expect_reject = [&](Flow flow) {
    try { validate_flow(model, flow); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("invalid persisted flow accepted");
  };
  expect_reject(Flow{"x", {"unknown"}});
  expect_reject(Flow{"x", {"finish", "finish"}});
  model.set_limits({1, 1, 1});
  expect_reject(Flow{"x", {"finish", "finish"}});
}

}  // namespace

int main() {
  try {
    linear_flow_is_terminal_and_deterministic();
    branch_respects_case_limit();
    self_loop_is_one_bounded_flow();
    non_self_cycle_can_repeat_within_step_limit();
    ordering_cycle_is_rejected();
    zero_cases_is_explicit();
    empty_model_emits_empty_flow();
    persisted_flow_validation_rejects_invalid_sequences();
  } catch (const std::exception& error) {
    std::cerr << "generator test failure: " << error.what() << '\n';
    return 1;
  }
  std::cout << "generator: all tests passed\n";
}
