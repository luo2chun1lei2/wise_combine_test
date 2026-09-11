#include "state_machine_path_generator.h"

#include <deque>
#include <random>

namespace spath {

namespace {

std::string transitionKey(const smodel::Transition &transition) {
  return transition.from + " -" + transition.event + "-> " + transition.to;
}

bool matchesFrom(const smodel::StateMachine &machine, const std::string &state,
                 const smodel::Transition &transition) {
  return state == transition.from || smodel::isDescendantOf(machine, state, transition.from);
}

std::string targetLeaf(const smodel::StateMachine &machine, const smodel::Transition &transition) {
  return smodel::leafOf(machine, transition.to);
}

}  // namespace

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
  dfs(smodel::leafOf(machine_, machine_.initial), path);
  return results_;
}

std::vector<Path> StateMachinePathGenerator::generateBfs() {
  struct Node {
    std::string state;
    Path path;
  };

  std::vector<Path> out;
  std::set<std::string> seen;
  std::deque<Node> queue;
  queue.push_back({smodel::leafOf(machine_, machine_.initial), {}});

  while (!queue.empty()) {
    Node node = queue.front();
    queue.pop_front();
    if (static_cast<int>(node.path.steps.size()) >= maxLength_) {
      continue;
    }
    for (const auto &transition : machine_.transitions) {
      if (!matchesFrom(machine_, node.state, transition)) {
        continue;
      }
      Path next = node.path;
      Step step;
      step.transition = transition;
      next.steps.push_back(step);
      if (seen.insert(next.text()).second) {
        out.push_back(next);
        queue.push_back({targetLeaf(machine_, transition), next});
      }
    }
  }
  return out;
}

std::vector<Path> StateMachinePathGenerator::generateRandom(unsigned seed, int count) {
  std::vector<Path> out;
  std::set<std::string> seen;
  std::mt19937 rng(seed);
  const int attempts = (count <= 0) ? 20 : count;

  for (int i = 0; i < attempts; ++i) {
    Path path;
    std::string state = smodel::leafOf(machine_, machine_.initial);
    while (static_cast<int>(path.steps.size()) < maxLength_) {
      std::vector<const smodel::Transition *> outgoing;
      for (const auto &transition : machine_.transitions) {
        if (matchesFrom(machine_, state, transition)) {
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
      state = targetLeaf(machine_, *transition);
    }
    if (!path.steps.empty() && seen.insert(path.text()).second) {
      out.push_back(path);
    }
  }
  return out;
}

std::vector<Path> StateMachinePathGenerator::generateTour() {
  std::vector<Path> out;
  if (machine_.transitions.empty()) {
    return out;
  }

  Path path;
  std::string state = smodel::leafOf(machine_, machine_.initial);
  std::set<std::string> covered;
  const std::size_t maxSteps = machine_.transitions.size() * 32 + 64;

  while (covered.size() < machine_.transitions.size() && path.steps.size() < maxSteps) {
    const smodel::Transition *pick = nullptr;
    for (const auto &transition : machine_.transitions) {
      if (matchesFrom(machine_, state, transition) &&
          covered.find(transitionKey(transition)) == covered.end()) {
        pick = &transition;
        break;
      }
    }
    if (pick == nullptr) {
      for (const auto &transition : machine_.transitions) {
        if (matchesFrom(machine_, state, transition)) {
          pick = &transition;
          break;
        }
      }
    }
    if (pick == nullptr) {
      break;
    }
    Step step;
    step.transition = *pick;
    path.steps.push_back(step);
    covered.insert(transitionKey(*pick));
    state = targetLeaf(machine_, *pick);
  }

  if (!path.steps.empty()) {
    out.push_back(path);
  }
  return out;
}

void StateMachinePathGenerator::dfs(const std::string &state, Path &path) {
  if (static_cast<int>(path.steps.size()) >= maxLength_) {
    return;
  }

  for (const auto &transition : machine_.transitions) {
    if (!matchesFrom(machine_, state, transition)) {
      continue;
    }

    Path next = path;
    Step step;
    step.transition = transition;
    next.steps.push_back(step);

    const std::string key = next.text();
    if (seen_.insert(key).second) {
      results_.push_back(next);
      dfs(targetLeaf(machine_, transition), next);
    }
  }
}

}  // namespace spath
