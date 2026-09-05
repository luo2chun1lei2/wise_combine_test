#include "model/model.hpp"

#include <functional>
#include <iostream>
#include <string>

namespace {

using wise::model::ArgumentRelation;
using wise::model::Function;
using wise::model::Model;
using wise::model::ModelError;
using wise::model::OrderingRelation;
using wise::model::Parameter;
using wise::model::ReturnValue;
using wise::model::State;
using wise::model::Transition;

void expect_error(const std::function<void()>& operation,
                  ModelError::Code expected) {
  try {
    operation();
  } catch (const ModelError& error) {
    if (error.code() != expected) {
      throw std::runtime_error("unexpected model error code");
    }
    return;
  }
  throw std::runtime_error("expected model error");
}

Model valid_model() {
  Model model;
  model.add_state(State{"idle"});
  model.add_state(State{"ready"});
  model.add_function(Function{"start", {Parameter{"input", "text"}},
                              {ReturnValue{"token", "text"}}});
  model.add_function(Function{"stop", {}, {}});
  model.add_function(Function{"consume", {Parameter{"input", "text"}}, {}});
  model.add_transition(Transition{"start-step", "idle", "ready", "start"});
  model.add_transition(Transition{"stop-step", "ready", "idle", "stop"});
  model.add_transition(Transition{"consume-step", "ready", "ready", "consume"});
  model.validate();
  return model;
}

void valid_graph_and_relations() {
  Model model = valid_model();
  model.add_argument_relation(ArgumentRelation{"start-step", "token",
                                               "consume-step", "input"});
  model.add_ordering_relation(OrderingRelation{"start-step", "stop-step"});
  model.validate();
}

void legal_state_self_loop_and_cycle() {
  Model model;
  model.add_state(State{"a"});
  model.add_state(State{"b"});
  model.add_function(Function{"tick", {}, {}});
  model.add_transition(Transition{"loop", "a", "a", "tick"});
  model.add_transition(Transition{"forward", "a", "b", "tick"});
  model.add_transition(Transition{"back", "b", "a", "tick"});
  model.validate();
}

void rejects_duplicate_and_unknown_references() {
  Model model;
  model.add_state(State{"a"});
  expect_error([&] { model.add_state(State{"a"}); }, ModelError::Code::duplicate_id);
  model.add_function(Function{"tick", {}, {}});
  model.add_transition(Transition{"step", "missing", "a", "tick"});
  expect_error([&] { model.validate(); }, ModelError::Code::unknown_reference);
}

void rejects_invalid_relations() {
  Model model = valid_model();
  expect_error([&] {
    model.add_argument_relation(ArgumentRelation{"start-step", "token",
                                                 "start-step", "input"});
  }, ModelError::Code::relation_self_edge);
  expect_error([&] {
    model.add_ordering_relation(OrderingRelation{"start-step", "start-step"});
  }, ModelError::Code::relation_self_edge);
  expect_error([&] {
    model.add_argument_relation(ArgumentRelation{"start-step", "token",
                                                 "consume-step", "input"});
    model.add_argument_relation(ArgumentRelation{"start-step", "token",
                                                 "consume-step", "input"});
    model.validate();
  }, ModelError::Code::duplicate_binding);
}

void rejects_type_mismatch_and_order_cycle() {
  Model model;
  model.add_state(State{"a"});
  model.add_function(Function{"producer", {}, {ReturnValue{"out", "number"}}});
  model.add_function(Function{"consumer", {Parameter{"in", "text"}}, {}});
  model.add_transition(Transition{"produce", "a", "a", "producer"});
  model.add_transition(Transition{"consume", "a", "a", "consumer"});
  model.add_argument_relation(ArgumentRelation{"produce", "out", "consume", "in"});
  expect_error([&] { model.validate(); }, ModelError::Code::type_mismatch);

  Model ordered = valid_model();
  ordered.add_ordering_relation(OrderingRelation{"start-step", "stop-step"});
  ordered.add_ordering_relation(OrderingRelation{"stop-step", "start-step"});
  expect_error([&] { ordered.validate(); }, ModelError::Code::contradictory_ordering);
}

}  // namespace

int main() {
  try {
    valid_graph_and_relations();
    legal_state_self_loop_and_cycle();
    rejects_duplicate_and_unknown_references();
    rejects_invalid_relations();
    rejects_type_mismatch_and_order_cycle();
  } catch (const std::exception& error) {
    std::cerr << "model test failure: " << error.what() << '\n';
    return 1;
  }
  std::cout << "model: all tests passed\n";
  return 0;
}
