#include "state_machine_path_generator.h"

#include <random>

namespace spath {

std::string Path::text() const {
  std::string out;
  for (std::size_t i = 0; i < steps.size(); ++i) {
    if (i > 0) {
      out += " ; ";
    }
    out += steps[i].transition.from;
    out += " -";
    out += steps[i].transition.event;
    out += "-> ";
    out += steps[i].transition.to;
  }
  return out;
}

StateMachinePathGenerator::StateMachinePathGenerator(const smodel::StateMachine &machine,
                                                     int maxLength)
    : machine_(machine), maxLength_(maxLength) {}

std::vector<Path> StateMachinePathGenerator::generate() {
  results_.clear();
  seen_.clear();
  Path path;
  dfs(machine_.initial, path);
  return results_;
}

std::vector<Path> StateMachinePathGenerator::generateRandom(unsigned seed, int count) {
  std::vector<Path> out;
  std::set<std::string> seen;
  std::mt19937 rng(seed);
  const int attempts = (count <= 0) ? 20 : count;

  for (int i = 0; i < attempts; ++i) {
    Path path;
    std::string state = machine_.initial;
    while (static_cast<int>(path.steps.size()) < maxLength_) {
      std::vector<const smodel::Transition *> outgoing;
      for (const auto &transition : machine_.transitions) {
        if (transition.from == state) {
          outgoing.push_back(&transition);
        }
      }
      if (outgoing.empty()) {
        break;
      }
      const smodel::Transition *transition = outgoing[rng() % outgoing.size()];
      Step step;
      step.transition = *transition;
      path.steps.push_back(step);
      state = transition->to;
    }
    if (!path.steps.empty() && seen.insert(path.text()).second) {
      out.push_back(path);
    }
  }
  return out;
}

void StateMachinePathGenerator::dfs(const std::string &state, Path &path) {
  if (static_cast<int>(path.steps.size()) >= maxLength_) {
    return;
  }

  for (const auto &transition : machine_.transitions) {
    if (transition.from != state) {
      continue;
    }

    Path next = path;
    Step step;
    step.transition = transition;
    next.steps.push_back(step);

    const std::string key = next.text();
    if (seen_.insert(key).second) {
      results_.push_back(next);
      dfs(transition.to, next);
    }
  }
}

}  // namespace spath
