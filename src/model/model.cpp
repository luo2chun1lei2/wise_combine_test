#include "model/model.hpp"

#include <algorithm>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace wise::model {
namespace {

template <typename T, typename Key>
bool contains_id(const std::vector<T>& values, const Key& id) {
  return std::any_of(values.begin(), values.end(),
                     [&](const T& value) { return value.id == id; });
}

[[noreturn]] void duplicate(const std::string& kind, const std::string& id) {
  throw ModelError(ModelError::Code::duplicate_id, kind + " already exists: " + id);
}

const Function& function_for(const std::vector<Function>& functions,
                             const std::string& id) {
  const auto it = std::find_if(functions.begin(), functions.end(),
                               [&](const Function& value) { return value.id == id; });
  if (it == functions.end()) {
    throw ModelError(ModelError::Code::unknown_reference, "unknown function: " + id);
  }
  return *it;
}

const Transition& transition_for(const std::vector<Transition>& transitions,
                                 const std::string& id) {
  const auto it = std::find_if(transitions.begin(), transitions.end(),
                               [&](const Transition& value) { return value.id == id; });
  if (it == transitions.end()) {
    throw ModelError(ModelError::Code::unknown_reference, "unknown transition: " + id);
  }
  return *it;
}

template <typename T>
const T* named(const std::vector<T>& values, const std::string& name) {
  const auto it = std::find_if(values.begin(), values.end(), [&](const T& value) {
    return value.name == name;
  });
  return it == values.end() ? nullptr : &*it;
}

}  // namespace

ModelError::ModelError(Code code, const std::string& message)
    : std::runtime_error(message), code_(code) {}

void Model::add_state(State state) {
  if (state.id.empty() || contains_id(states_, state.id)) {
    duplicate("state", state.id);
  }
  states_.push_back(std::move(state));
}

void Model::add_function(Function function) {
  if (function.id.empty() || contains_id(functions_, function.id)) {
    duplicate("function", function.id);
  }
  std::unordered_set<std::string> names;
  for (const auto& parameter : function.parameters) {
    if (parameter.name.empty() || parameter.type.empty() ||
        !names.insert(parameter.name).second) {
      throw ModelError(ModelError::Code::invalid_argument,
                       "invalid or duplicate function parameter: " + parameter.name);
    }
  }
  for (const auto& output : function.returns) {
    if (output.name.empty() || output.type.empty() ||
        !names.insert(output.name).second) {
      throw ModelError(ModelError::Code::invalid_argument,
                       "invalid or duplicate function return: " + output.name);
    }
  }
  functions_.push_back(std::move(function));
}

void Model::add_transition(Transition transition) {
  if (transition.id.empty() || contains_id(transitions_, transition.id)) {
    duplicate("transition", transition.id);
  }
  transitions_.push_back(std::move(transition));
}

void Model::add_argument_relation(ArgumentRelation relation) {
  if (relation.producer_transition == relation.consumer_transition) {
    throw ModelError(ModelError::Code::relation_self_edge,
                     "argument relation cannot reference one transition twice");
  }
  argument_relations_.push_back(std::move(relation));
}

void Model::add_ordering_relation(OrderingRelation relation) {
  if (relation.before == relation.after) {
    throw ModelError(ModelError::Code::relation_self_edge,
                     "ordering relation cannot order a transition before itself");
  }
  ordering_relations_.push_back(std::move(relation));
}

void Model::set_initial_state(std::string state_id) { initial_state_ = std::move(state_id); }

void Model::set_limits(Limits limits) {
  if (limits.max_cases == 0U || limits.max_steps == 0U || limits.max_subprocesses == 0U) {
    throw ModelError(ModelError::Code::invalid_limits, "model limits must be positive");
  }
  limits_ = limits;
}

void Model::validate() const {
  if (!initial_state_.empty() && !contains_id(states_, initial_state_)) {
    throw ModelError(ModelError::Code::unknown_reference,
                     "unknown initial state: " + initial_state_);
  }
  for (const auto& transition : transitions_) {
    if (!contains_id(states_, transition.from) || !contains_id(states_, transition.to)) {
      throw ModelError(ModelError::Code::unknown_reference,
                       "transition references unknown state: " + transition.id);
    }
    static_cast<void>(function_for(functions_, transition.function));
  }

  std::unordered_set<std::string> bound_arguments;
  for (const auto& relation : argument_relations_) {
    const auto& producer_transition = transition_for(transitions_, relation.producer_transition);
    const auto& consumer_transition = transition_for(transitions_, relation.consumer_transition);
    const auto& producer_function = function_for(functions_, producer_transition.function);
    const auto& consumer_function = function_for(functions_, consumer_transition.function);
    const auto* output = named(producer_function.returns, relation.producer_output);
    const auto* argument = named(consumer_function.parameters, relation.consumer_argument);
    if (output == nullptr || argument == nullptr) {
      throw ModelError(ModelError::Code::unknown_reference,
                       "argument relation names an unknown output or parameter");
    }
    if (output->type != argument->type) {
      throw ModelError(ModelError::Code::type_mismatch,
                       "argument relation types do not match");
    }
    const auto key = relation.consumer_transition + "\x1f" + relation.consumer_argument;
    if (!bound_arguments.insert(key).second) {
      throw ModelError(ModelError::Code::duplicate_binding,
                       "consumer argument has multiple producers");
    }
  }

  std::unordered_map<std::string, std::vector<std::string>> edges;
  for (const auto& relation : ordering_relations_) {
    static_cast<void>(transition_for(transitions_, relation.before));
    static_cast<void>(transition_for(transitions_, relation.after));
    edges[relation.before].push_back(relation.after);
  }
  std::unordered_map<std::string, unsigned char> marks;
  std::function<void(const std::string&)> visit = [&](const std::string& node) {
    const auto mark = marks[node];
    if (mark == 1U) {
      throw ModelError(ModelError::Code::contradictory_ordering,
                       "ordering relations contain a cycle");
    }
    if (mark == 2U) return;
    marks[node] = 1U;
    for (const auto& next : edges[node]) visit(next);
    marks[node] = 2U;
  };
  for (const auto& transition : transitions_) visit(transition.id);
}

}  // namespace wise::model
