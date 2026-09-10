#pragma once

#include <set>
#include <string>
#include <vector>

#include "state_machine_model.h"

namespace spath {

struct Step {
  smodel::Transition transition;
};

struct Path {
  std::vector<Step> steps;
  std::string text() const;
};

class StateMachinePathGenerator {
 public:
  StateMachinePathGenerator(const smodel::StateMachine &machine, int maxLength);
  std::vector<Path> generate();
  std::vector<Path> generateRandom(unsigned seed, int count);

 private:
  const smodel::StateMachine &machine_;
  int maxLength_ = 0;
  std::vector<Path> results_;
  std::set<std::string> seen_;

  void dfs(const std::string &state, Path &path);
};

}  // namespace spath
