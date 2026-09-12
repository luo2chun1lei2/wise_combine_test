#pragma once

#include <map>
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
  StateMachinePathGenerator(const smodel::StateMachine &machine, int maxLength,
                            const std::map<std::string, std::string> &guardValues = {},
                            int maxCases = 0);
  std::vector<Path> generate();
  std::vector<Path> generateBfs();
  std::vector<Path> generateRandom(unsigned seed, int count);
  std::vector<Path> generateTour();

  const std::set<std::string> &skippedGuards() const { return skippedGuards_; }
  const std::set<std::string> &uncoveredTransitions() const {
    return uncoveredTransitions_;
  }
  bool truncated() const { return truncated_; }

 private:
  const smodel::StateMachine &machine_;
  int maxLength_ = 0;
  std::map<std::string, std::string> guardValues_;
  int maxCases_ = 0;
  std::vector<Path> results_;
  std::set<std::string> seen_;
  std::set<std::string> skippedGuards_;
  std::set<std::string> uncoveredTransitions_;
  bool truncated_ = false;

  bool reachedLimit() const;
  bool canFire(const std::set<std::string> &active, const smodel::Transition &transition);
  void dfs(const std::set<std::string> &active, Path &path);
};

}  // namespace spath
