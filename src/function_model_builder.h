#pragma once

#include <set>

#include "FunctionDslBaseVisitor.h"
#include "model.h"

class FunctionModelBuilder : public FunctionDslBaseVisitor {
 public:
  model::Model build(FunctionDslParser::ModelContext *ctx);

  std::any visitTypeMap(FunctionDslParser::TypeMapContext *ctx) override;
  std::any visitValueSource(FunctionDslParser::ValueSourceContext *ctx) override;
  std::any visitResourceBlock(FunctionDslParser::ResourceBlockContext *ctx) override;
  std::any visitFuncBlock(FunctionDslParser::FuncBlockContext *ctx) override;
  std::any visitSetupBlock(FunctionDslParser::SetupBlockContext *ctx) override;
  std::any visitClassBlock(FunctionDslParser::ClassBlockContext *ctx) override;

 private:
  model::Model model_;
  std::set<std::string> seenTypes_;
  std::set<std::string> seenValues_;
  std::set<std::string> seenResources_;
  std::set<std::string> seenClasses_;
  std::set<std::string> seenSetups_;

  void validate();
  static std::string unquote(const std::string &s);
  static std::string operandText(FunctionDslParser::OperandContext *ctx);
};
