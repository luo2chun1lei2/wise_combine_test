#pragma once

#include <map>
#include <string>
#include <vector>

namespace smodel {

struct Transition {
  std::string from;
  std::string to;
  std::string event;
  std::string guard;
  std::string action;
};

struct StateInfo {
  std::string entry;
  std::string exit;
};

struct StateMachine {
  std::string name;
  std::vector<std::string> states;
  std::string initial;
  std::vector<std::string> events;
  std::map<std::string, StateInfo> stateInfo;
  std::vector<Transition> transitions;
  std::vector<std::string> errors;
};

}  // namespace smodel
