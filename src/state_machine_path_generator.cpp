#include "state_machine_path_generator.h"

#include <deque>
#include <random>

#include "guard.h"

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

StateMachinePathGenerator::StateMachinePathGenerator(
    const smodel::StateMachine &machine, int maxLength,
    const std::map<std::string, std::string> &guardValues, int maxCases)
    : machine_(machine),
      maxLength_(maxLength),
      guardValues_(guardValues),
      maxCases_(maxCases) {}

bool StateMachinePathGenerator::reachedLimit() const {
  return maxCases_ > 0 && static_cast<int>(results_.size()) >= maxCases_;
}

bool StateMachinePathGenerator::canFire(const std::set<std::string> &active,
                                        const smodel::Transition &transition) {
  if (!matches(machine_, active, transition)) {
    return false;
  }
  if (transition.guard.empty()) {
    return true;
  }

  std::string error;
  const bool ok = guard::evalGuard(transition.guard, guardValues_, &error);
  if (!ok && !error.empty()) {
    skippedGuards_.insert(transition.from + " -" + transition.event + "-> " +
                          transition.to + " (" + error + ")");
  }
  return ok;
}

std::vector<Path> StateMachinePathGenerator::generate() {
  results_.clear();
  seen_.clear();
  skippedGuards_.clear();
  Path path;
  dfs(smodel::enterLeaves(machine_, machine_.initial), path);
  return results_;
}

std::vector<Path> StateMachinePathGenerator::generateBfs() {
  struct Node {
    Active active;
    Path path;
  };

  std::deque<Node> queue;
  queue.push_back({smodel::enterLeaves(machine_, machine_.initial), {}});

  results_.clear();
  seen_.clear();
  skippedGuards_.clear();

  while (!queue.empty()) {
    if (reachedLimit()) {
      break;
    }
    Node node = queue.front();
    queue.pop_front();
    if (static_cast<int>(node.path.steps.size()) >= maxLength_) {
      continue;
    }
    for (const auto &transition : machine_.transitions) {
      if (!canFire(node.active, transition)) {
        continue;
      }
      Path next = node.path;
      Step step;
      step.transition = transition;
      next.steps.push_back(step);
      if (seen_.insert(next.text()).second) {
        results_.push_back(next);
        queue.push_back({fire(machine_, node.active, transition), next});
        if (reachedLimit()) {
          break;
        }
      }
    }
  }
  return results_;
}

std::vector<Path> StateMachinePathGenerator::generateRandom(unsigned seed, int count) {
  std::mt19937 rng(seed);
  const int attempts = (count <= 0) ? 20 : count;

  results_.clear();
  seen_.clear();
  skippedGuards_.clear();

  for (int i = 0; i < attempts; ++i) {
    if (reachedLimit()) {
      break;
    }
    Path path;
    Active active = smodel::enterLeaves(machine_, machine_.initial);
    while (static_cast<int>(path.steps.size()) < maxLength_) {
      std::vector<const smodel::Transition *> outgoing;
      for (const auto &transition : machine_.transitions) {
        if (canFire(active, transition)) {
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
    if (!path.steps.empty() && seen_.insert(path.text()).second) {
      results_.push_back(path);
    }
  }
  return results_;
}

std::vector<Path> StateMachinePathGenerator::generateTour() {
  std::vector<Path> out;
  uncoveredTransitions_.clear();
  truncated_ = false;
  if (machine_.transitions.empty()) {
    return out;
  }

  Path path;
  Active active = smodel::enterLeaves(machine_, machine_.initial);
  std::set<std::string> covered;
  const std::size_t maxSteps =
      maxLength_ > 0 ? static_cast<std::size_t>(maxLength_)
                     : machine_.transitions.size() * 32 + 64;

  while (covered.size() < machine_.transitions.size() && path.steps.size() < maxSteps) {
    const smodel::Transition *pick = nullptr;
    for (const auto &transition : machine_.transitions) {
      if (canFire(active, transition) &&
          covered.find(transitionKey(transition)) == covered.end()) {
        pick = &transition;
        break;
      }
    }
    if (pick == nullptr) {
      for (const auto &transition : machine_.transitions) {
        if (canFire(active, transition)) {
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

  if (covered.size() < machine_.transitions.size()) {
    truncated_ = true;
    for (const auto &transition : machine_.transitions) {
      if (covered.find(transitionKey(transition)) == covered.end()) {
        uncoveredTransitions_.insert(transitionKey(transition));
      }
    }
  }
  return out;
}

void StateMachinePathGenerator::dfs(const Active &active, Path &path) {
  if (reachedLimit() || static_cast<int>(path.steps.size()) >= maxLength_) {
    return;
  }

  for (const auto &transition : machine_.transitions) {
    if (reachedLimit()) {
      return;
    }
    if (!canFire(active, transition)) {
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
