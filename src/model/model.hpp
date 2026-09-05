#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wise::model {

struct State {
  std::string id;
};

struct Parameter {
  std::string name;
  std::string type;
};

struct ReturnValue {
  std::string name;
  std::string type;
};

struct Function {
  std::string id;
  std::vector<Parameter> parameters;
  std::vector<ReturnValue> returns;
};

struct Scalar {
  enum class Kind { null_value, boolean, integer, number, string };
  Kind kind{Kind::null_value};
  bool boolean_value{false};
  std::int64_t integer_value{0};
  double number_value{0.0};
  std::string string_value;
};

struct Transition {
  Transition() = default;
  Transition(std::string id_value, std::string from_value, std::string to_value,
             std::string function_value,
             std::map<std::string, Scalar> args_value = {},
             bool expect_present_value = false, std::string expect_value = {})
      : id(std::move(id_value)),
        from(std::move(from_value)),
        to(std::move(to_value)),
        function(std::move(function_value)),
        args(std::move(args_value)),
        expect_present(expect_present_value),
        expect(std::move(expect_value)) {}

  std::string id;
  std::string from;
  std::string to;
  std::string function;
  std::map<std::string, Scalar> args;
  bool expect_present{false};
  std::string expect;
};

struct ArgumentRelation {
  std::string producer_transition;
  std::string producer_output;
  std::string consumer_transition;
  std::string consumer_argument;
};

struct OrderingRelation {
  std::string before;
  std::string after;
};

struct Limits {
  std::size_t max_cases{1};
  std::size_t max_steps{1};
  std::size_t max_subprocesses{1};
};

enum class OutcomeStatus { ok, mismatch, error };

struct ObservedOutcome {
  OutcomeStatus status{OutcomeStatus::ok};
  std::string observed_state;
  std::string stderr_output;
};

class ModelError final : public std::runtime_error {
 public:
  enum class Code {
    duplicate_id,
    unknown_reference,
    relation_self_edge,
    duplicate_binding,
    type_mismatch,
    contradictory_ordering,
    invalid_argument,
    invalid_limits,
  };

  ModelError(Code code, const std::string& message);
  [[nodiscard]] Code code() const noexcept { return code_; }

 private:
  Code code_;
};

class Model final {
 public:
  Model() = default;

  void add_state(State state);
  void add_function(Function function);
  void add_transition(Transition transition);
  void add_argument_relation(ArgumentRelation relation);
  void add_ordering_relation(OrderingRelation relation);
  void set_initial_state(std::string state_id);
  void set_limits(Limits limits);

  void validate() const;

  [[nodiscard]] const std::vector<State>& states() const noexcept { return states_; }
  [[nodiscard]] const std::vector<Function>& functions() const noexcept {
    return functions_;
  }
  [[nodiscard]] const std::vector<Transition>& transitions() const noexcept {
    return transitions_;
  }
  [[nodiscard]] const std::vector<ArgumentRelation>& argument_relations() const noexcept {
    return argument_relations_;
  }
  [[nodiscard]] const std::vector<OrderingRelation>& ordering_relations() const noexcept {
    return ordering_relations_;
  }
  [[nodiscard]] const Limits& limits() const noexcept { return limits_; }
  [[nodiscard]] const std::string& initial_state() const noexcept {
    return initial_state_;
  }

 private:
  std::vector<State> states_;
  std::vector<Function> functions_;
  std::vector<Transition> transitions_;
  std::vector<ArgumentRelation> argument_relations_;
  std::vector<OrderingRelation> ordering_relations_;
  std::string initial_state_;
  Limits limits_{};
};

}  // namespace wise::model
