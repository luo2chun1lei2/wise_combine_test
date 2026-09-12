#include "function_model_builder.h"

#include <set>

namespace {

bool isBuiltinType(const std::string &name) {
  return name == "string" || name == "int" || name == "bool";
}

bool contains(const std::vector<std::string> &vec, const std::string &value) {
  for (const auto &v : vec) {
    if (v == value) {
      return true;
    }
  }
  return false;
}

}  // namespace

model::Model FunctionModelBuilder::build(FunctionDslParser::ModelContext *ctx) {
  if (ctx != nullptr) {
    ctx->accept(this);
  }
  validate();
  return model_;
}

std::any FunctionModelBuilder::visitTypeMap(FunctionDslParser::TypeMapContext *ctx) {
  const std::string dslType = ctx->ID()->getText();
  if (!seenTypes_.insert(dslType).second) {
    model_.errors.push_back("duplicate type: " + dslType);
  }
  const std::string cType = unquote(ctx->STRING()->getText());
  model_.typeMap[dslType] = cType;
  return nullptr;
}

std::any FunctionModelBuilder::visitValueSource(FunctionDslParser::ValueSourceContext *ctx) {
  model::ValueSource source;
  source.name = ctx->ID()->getText();
  if (!seenValues_.insert(source.name).second) {
    model_.errors.push_back("duplicate value source: " + source.name);
  }

  if (ctx->list() != nullptr) {
    source.isRange = false;
    for (auto *str : ctx->list()->STRING()) {
      source.items.push_back(unquote(str->getText()));
    }
  } else if (ctx->range() != nullptr) {
    source.isRange = true;
    source.lo = std::stoi(ctx->range()->INT(0)->getText());
    source.hi = std::stoi(ctx->range()->INT(1)->getText());
  }

  model_.values[source.name] = source;
  return nullptr;
}

std::any FunctionModelBuilder::visitResourceBlock(FunctionDslParser::ResourceBlockContext *ctx) {
  model::Resource resource;
  resource.name = ctx->ID()->getText();
  if (!seenResources_.insert(resource.name).second) {
    model_.errors.push_back("duplicate resource: " + resource.name);
  }

  for (auto *decl : ctx->ctypeDecl()) {
    resource.ctype = unquote(decl->STRING()->getText());
  }
  for (auto *decl : ctx->statesDecl()) {
    for (auto *id : decl->ID()) {
      resource.states.push_back(id->getText());
    }
  }
  for (auto *decl : ctx->initialDecl()) {
    resource.initial = decl->ID()->getText();
  }
  for (auto *decl : ctx->observeDecl()) {
    resource.observe = decl->ID()->getText();
  }

  model_.resources[resource.name] = resource;
  return nullptr;
}

std::any FunctionModelBuilder::visitFuncBlock(FunctionDslParser::FuncBlockContext *ctx) {
  model::Function function;
  function.name = ctx->ID()->getText();

  if (ctx->params() != nullptr) {
    for (auto *paramCtx : ctx->params()->param()) {
      model::Param param;
      param.name = paramCtx->ID(0)->getText();
      param.type = paramCtx->typeName()->ID()->getText();
      param.out = paramCtx->getText().rfind("out", 0) == 0;
      if (paramCtx->ID().size() > 1) {
        param.valueSource = paramCtx->ID(1)->getText();
      }
      function.params.push_back(param);
    }
  }

  if (ctx->typeName() != nullptr) {
    function.returnType = ctx->typeName()->ID()->getText();
  }

  for (auto *member : ctx->funcMember()) {
    if (member->symbolDecl() != nullptr) {
      function.symbol = unquote(member->symbolDecl()->STRING()->getText());
    } else if (member->signatureDecl() != nullptr) {
      function.signature = unquote(member->signatureDecl()->STRING()->getText());
    } else if (member->requiresDecl() != nullptr) {
      for (auto *condCtx : member->requiresDecl()->cond()) {
        model::Cond cond;
        cond.param = condCtx->ID(0)->getText();
        cond.state = condCtx->ID(1)->getText();
        function.requiresConds.push_back(cond);
      }
    } else if (member->effectsDecl() != nullptr) {
      for (auto *effectCtx : member->effectsDecl()->effect()) {
        model::Effect effect;
        if (effectCtx->target()->ID() != nullptr) {
          effect.target = effectCtx->target()->ID()->getText();
        } else {
          effect.target = "result";
        }
        effect.state = effectCtx->ID()->getText();
        function.effects.push_back(effect);
      }
    } else if (member->successDecl() != nullptr) {
      function.success.expr = member->successDecl()->successExpr()->getText();
    } else if (member->receiverDecl() != nullptr) {
      function.receiver = member->receiverDecl()->ID()->getText();
    }
  }

  model_.functions.push_back(function);
  return nullptr;
}

std::any FunctionModelBuilder::visitSetupBlock(FunctionDslParser::SetupBlockContext *ctx) {
  for (auto *entry : ctx->setupEntry()) {
    model::SetupEntry setup;
    setup.name = entry->ID(0)->getText();
    if (!seenSetups_.insert(setup.name).second) {
      model_.errors.push_back("duplicate setup: " + setup.name);
    }
    setup.function = entry->ID(1)->getText();
    for (auto *arg : entry->setupArg()) {
      if (arg->STRING() != nullptr) {
        setup.args.push_back(unquote(arg->STRING()->getText()));
      } else if (arg->INT() != nullptr) {
        setup.args.push_back(arg->INT()->getText());
      }
    }
    setup.count = std::stoi(entry->INT()->getText());
    model_.setups.push_back(setup);
  }
  return nullptr;
}

std::any FunctionModelBuilder::visitClassBlock(FunctionDslParser::ClassBlockContext *ctx) {
  model::ClassEntry entry;
  entry.name = ctx->ID()->getText();
  if (!seenClasses_.insert(entry.name).second) {
    model_.errors.push_back("duplicate class: " + entry.name);
  }
  entry.cpp = unquote(ctx->cppDecl()->STRING()->getText());
  entry.header = unquote(ctx->headerDecl()->STRING()->getText());
  model_.classes[entry.name] = entry;
  return nullptr;
}

std::string FunctionModelBuilder::unquote(const std::string &s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

std::string FunctionModelBuilder::operandText(FunctionDslParser::OperandContext *ctx) {
  if (ctx->ID() != nullptr) {
    return ctx->ID()->getText();
  }
  if (ctx->INT() != nullptr) {
    return ctx->INT()->getText();
  }
  return ctx->getText();
}

void FunctionModelBuilder::validate() {
  std::set<std::string> resourceNames;
  std::set<std::string> functionNames;

  for (const auto &[name, resource] : model_.resources) {
    if (!resourceNames.insert(name).second) {
      model_.errors.push_back("duplicate resource: " + name);
    }
    if (resource.states.empty()) {
      model_.errors.push_back("resource " + name + " has no states");
    }
    if (resource.ctype.empty()) {
      model_.errors.push_back("resource " + name + " has no ctype");
    }
    if (!resource.initial.empty() && !contains(resource.states, resource.initial)) {
      model_.errors.push_back("resource " + name + " initial state not declared: " + resource.initial);
    }
  }

  for (const auto &function : model_.functions) {
    if (!functionNames.insert(function.name).second) {
      model_.errors.push_back("duplicate function: " + function.name);
    }
    if (function.symbol.empty()) {
      model_.errors.push_back("function " + function.name + " has no symbol");
    }
    if (!function.receiver.empty() && model_.classes.find(function.receiver) == model_.classes.end()) {
      model_.errors.push_back("function " + function.name + " references unknown class: " + function.receiver);
    }

    for (const auto &param : function.params) {
      if (!isBuiltinType(param.type) &&
          model_.resources.find(param.type) == model_.resources.end() &&
          model_.typeMap.find(param.type) == model_.typeMap.end()) {
        model_.errors.push_back("function " + function.name + " param " + param.name +
                                " has unknown type: " + param.type);
      }
      if (!param.valueSource.empty() && model_.values.find(param.valueSource) == model_.values.end()) {
        model_.errors.push_back("function " + function.name + " param " + param.name +
                                " references unknown value source: " + param.valueSource);
      }
    }

    if (!function.returnType.empty() && !isBuiltinType(function.returnType) &&
        model_.resources.find(function.returnType) == model_.resources.end() &&
        model_.typeMap.find(function.returnType) == model_.typeMap.end()) {
      model_.errors.push_back("function " + function.name + " has unknown return type: " + function.returnType);
    }

    for (const auto &cond : function.requiresConds) {
      bool found = false;
      std::string paramType;
      for (const auto &param : function.params) {
        if (param.name == cond.param) {
          found = true;
          paramType = param.type;
          break;
        }
      }
      if (!found) {
        model_.errors.push_back("function " + function.name + " requires unknown param: " + cond.param);
        continue;
      }
      auto it = model_.resources.find(paramType);
      if (it != model_.resources.end() && !contains(it->second.states, cond.state)) {
        model_.errors.push_back("function " + function.name + " requires unknown state: " + cond.state);
      }
    }

    for (const auto &effect : function.effects) {
      if (effect.target == "result") {
        continue;
      }
      bool found = false;
      std::string paramType;
      for (const auto &param : function.params) {
        if (param.name == effect.target) {
          found = true;
          paramType = param.type;
          break;
        }
      }
      if (!found) {
        model_.errors.push_back("function " + function.name + " effect targets unknown param: " + effect.target);
        continue;
      }
      auto it = model_.resources.find(paramType);
      if (it != model_.resources.end() && !contains(it->second.states, effect.state)) {
        model_.errors.push_back("function " + function.name + " effect uses unknown state: " + effect.state);
      }
    }
  }

  for (const auto &setup : model_.setups) {
    if (setup.count <= 0) {
      model_.errors.push_back("setup " + setup.name + " has non-positive count");
    }
    if (functionNames.find(setup.function) == functionNames.end()) {
      model_.errors.push_back("setup " + setup.name + " references unknown function: " + setup.function);
    }
  }
}
