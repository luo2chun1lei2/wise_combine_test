#pragma once

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

 private:
  model::Model model_;

  void validate();
  static std::string unquote(const std::string &s);
  static std::string operandText(FunctionDslParser::OperandContext *ctx);
};
