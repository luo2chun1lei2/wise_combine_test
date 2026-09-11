#include "state_machine_path_generator.h"

#include <deque>
#include <random>

namespace spath {

namespace {

using Active = std::set<std::string>;

std::string transitionKey(const smodel::Transition &transition) {
  return transition.from + " -" + transition.event + "-> " + transition.to;
}

bool matches(const smodel::StateMachine &machine, const Active &active,
             const smodel::Transition &transition) {
  for (const auto &leaf : active) {
    if (leaf == transition.from || smodel::isDescendantOf(machine, leaf, transition.from)) {
      return true;
    }
  }
  return false;
}

Active fire(const smodel::StateMachine &machine, const Active &active,
            const smodel::Transition &transition) {
  Active next = active;
  for (auto it = next.begin(); it != next.end();) {
    if (*it == transition.from || smodel::isDescendantOf(machine, *it, transition.from)) {
      it = next.erase(it);
    } else {
      ++it;
    }
  }
  for (const auto &leaf : smodel::enterLeaves(machine, transition.to)) {
    next.insert(leaf);
  }
  return next;
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
  dfs(smodel::enterLeaves(machine_, machine_.initial), path);
  return results_;
}

std::vector<Path> StateMachinePathGenerator::generateBfs() {
  struct Node {
    Active active;
    Path path;
  };

  std::vector<Path> out;
  std::set<std::string> seen;
  std::deque<Node> queue;
  queue.push_back({smodel::enterLeaves(machine_, machine_.initial), {}});

  while (!queue.empty()) {
    Node node = queue.front();
    queue.pop_front();
    if (static_cast<int>(node.path.steps.size()) >= maxLength_) {
      continue;
    }
    for (const auto &transition : machine_.transitions) {
      if (!matches(machine_, node.active, transition)) {
        continue;
      }
      Path next = node.path;
      Step step;
      step.transition = transition;
      next.steps.push_back(step);
      if (seen.insert(next.text()).second) {
        out.push_back(next);
        queue.push_back({fire(machine_, node.active, transition), next});
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
    Active active = smodel::enterLeaves(machine_, machine_.initial);
    while (static_cast<int>(path.steps.size()) < maxLength_) {
      std::vector<const smodel::Transition *> outgoing;
      for (const auto &transition : machine_.transitions) {
        if (matches(machine_, active, transition)) {
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
      active = fire(machine_, active, *transition);
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
  Active active = smodel::enterLeaves(machine_, machine_.initial);
  std::set<std::string> covered;
  const std::size_t maxSteps = machine_.transitions.size() * 32 + 64;

  while (covered.size() < machine_.transitions.size() && path.steps.size() < maxSteps) {
    const smodel::Transition *pick = nullptr;
    for (const auto &transition : machine_.transitions) {
      if (matches(machine_, active, transition) &&
          covered.find(transitionKey(transition)) == covered.end()) {
        pick = &transition;
        break;
      }
    }
    if (pick == nullptr) {
      for (const auto &transition : machine_.transitions) {
        if (matches(machine_, active, transition)) {
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
    active = fire(machine_, active, *pick);
  }

  if (!path.steps.empty()) {
    out.push_back(path);
  }
  return out;
}

void StateMachinePathGenerator::dfs(const Active &active, Path &path) {
  if (static_cast<int>(path.steps.size()) >= maxLength_) {
    return;
  }

  for (const auto &transition : machine_.transitions) {
    if (!matches(machine_, active, transition)) {
      continue;
    }
    Path next = path;
    Step step;
    step.transition = transition;
    next.steps.push_back(step);

    if (seen_.insert(next.text()).second) {
      results_.push_back(next);
      dfs(fire(machine_, active, transition), next);
    }
  }
}

}  // namespace spath
