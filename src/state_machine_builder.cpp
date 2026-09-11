#include "state_machine_builder.h"

#include <set>

namespace {

bool contains(const std::vector<std::string> &vec, const std::string &value) {
  for (const auto &v : vec) {
    if (v == value) {
      return true;
    }
  }
  return false;
}

}  // namespace

smodel::StateMachine StateMachineBuilder::build(StateMachineDslParser::MachineContext *ctx) {
  if (ctx != nullptr) {
    machine_.name = ctx->ID()->getText();
    ctx->accept(this);
  }
  validate();
  return machine_;
}

std::any StateMachineBuilder::visitStatesDecl(StateMachineDslParser::StatesDeclContext *ctx) {
  for (auto *id : ctx->ID()) {
    machine_.states.push_back(id->getText());
  }
  return nullptr;
}

std::any StateMachineBuilder::visitInitialDecl(StateMachineDslParser::InitialDeclContext *ctx) {
  machine_.initial = ctx->ID()->getText();
  return nullptr;
}

std::any StateMachineBuilder::visitEventsDecl(StateMachineDslParser::EventsDeclContext *ctx) {
  for (auto *id : ctx->ID()) {
    machine_.events.push_back(id->getText());
  }
  return nullptr;
}

std::any StateMachineBuilder::visitStateBlock(StateMachineDslParser::StateBlockContext *ctx) {
  smodel::StateInfo info;
  const std::string name = ctx->ID()->getText();
  info.parent = currentParent_;
  for (auto *entry : ctx->entryDecl()) {
    info.entry = actionText(entry->action());
  }
  for (auto *exit : ctx->exitDecl()) {
    info.exit = actionText(exit->action());
  }
  if (!ctx->initialDecl().empty()) {
    info.initial = ctx->initialDecl(0)->ID()->getText();
  }
  if (!ctx->historyDecl().empty()) {
    info.history = ctx->historyDecl(0)->ID()->getText();
  }
  machine_.stateInfo[name] = info;
  if (!currentParent_.empty()) {
    machine_.stateInfo[currentParent_].children.push_back(name);
  }

  const std::string savedParent = currentParent_;
  currentParent_ = name;
  for (auto *child : ctx->stateBlock()) {
    child->accept(this);
  }
  currentParent_ = savedParent;
  return nullptr;
}

std::any StateMachineBuilder::visitTransition(StateMachineDslParser::TransitionContext *ctx) {
  smodel::Transition transition;
  transition.from = ctx->ID(0)->getText();
  transition.to = ctx->ID(1)->getText();
  transition.event = ctx->ID(2)->getText();
  if (!ctx->guardDecl().empty()) {
    transition.guard = ctx->guardDecl(0)->expr()->getText();
  }
  if (!ctx->actionDecl().empty()) {
    transition.action = actionText(ctx->actionDecl(0)->action());
  }
  machine_.transitions.push_back(transition);
  return nullptr;
}

std::string StateMachineBuilder::actionText(StateMachineDslParser::ActionContext *ctx) {
  std::string out;
  for (std::size_t i = 0; i < ctx->ID().size(); ++i) {
    if (i > 0) {
      out += ".";
    }
    out += ctx->ID(i)->getText();
  }
  return out;
}

void StateMachineBuilder::validate() {
  if (!machine_.initial.empty() && !contains(machine_.states, machine_.initial)) {
    machine_.errors.push_back("initial state not declared: " + machine_.initial);
  }

  std::set<std::string> uniqueStates;
  for (const auto &state : machine_.states) {
    if (!uniqueStates.insert(state).second) {
      machine_.errors.push_back("duplicate state: " + state);
    }
  }

  for (const auto &transition : machine_.transitions) {
    if (!contains(machine_.states, transition.from)) {
      machine_.errors.push_back("transition from unknown state: " + transition.from);
    }
    if (!contains(machine_.states, transition.to)) {
      machine_.errors.push_back("transition to unknown state: " + transition.to);
    }
    if (!machine_.events.empty() && !contains(machine_.events, transition.event)) {
      machine_.errors.push_back("transition uses unknown event: " + transition.event);
    }
  }
}
