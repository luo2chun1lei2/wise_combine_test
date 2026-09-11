#pragma once

#include "StateMachineDslBaseVisitor.h"
#include "state_machine_model.h"

class StateMachineBuilder : public StateMachineDslBaseVisitor {
 public:
  smodel::StateMachine build(StateMachineDslParser::MachineContext *ctx);

  std::any visitStatesDecl(StateMachineDslParser::StatesDeclContext *ctx) override;
  std::any visitInitialDecl(StateMachineDslParser::InitialDeclContext *ctx) override;
  std::any visitEventsDecl(StateMachineDslParser::EventsDeclContext *ctx) override;
  std::any visitStateBlock(StateMachineDslParser::StateBlockContext *ctx) override;
  std::any visitTransition(StateMachineDslParser::TransitionContext *ctx) override;
  std::any visitClassBlock(StateMachineDslParser::ClassBlockContext *ctx) override;
  std::any visitActionsBlock(StateMachineDslParser::ActionsBlockContext *ctx) override;

 private:
  smodel::StateMachine machine_;
  std::string currentParent_;

  void validate();
  static std::string actionText(StateMachineDslParser::ActionContext *ctx);
};
