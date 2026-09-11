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
  std::string parent;
  std::vector<std::string> children;
  std::string initial;
  std::string history;
  std::string historyDeep;
  bool concurrent = false;
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

inline const StateInfo *findState(const StateMachine &machine, const std::string &name) {
  auto it = machine.stateInfo.find(name);
  return it == machine.stateInfo.end() ? nullptr : &it->second;
}

inline std::string leafOf(const StateMachine &machine, const std::string &name) {
  std::string current = name;
  for (int guard = 0; guard < 100; ++guard) {
    const StateInfo *info = findState(machine, current);
    if (info == nullptr || info->children.empty() || info->initial.empty()) {
      break;
    }
    current = info->initial;
  }
  return current;
}

inline bool isDescendantOf(const StateMachine &machine, const std::string &state,
                           const std::string &ancestor) {
  std::string current = state;
  for (int guard = 0; guard < 100; ++guard) {
    if (current == ancestor) {
      return true;
    }
    const StateInfo *info = findState(machine, current);
    if (info == nullptr || info->parent.empty()) {
      break;
    }
    current = info->parent;
  }
  return false;
}

}  // namespace smodel
