#include "harness_generator.h"

#include <algorithm>
#include <map>
#include <sstream>

namespace harness {

namespace {

bool isResource(const model::Model &model, const std::string &type) {
  return model.resources.find(type) != model.resources.end();
}

std::string cTypeOf(const model::Model &model, const std::string &type) {
  auto res = model.resources.find(type);
  if (res != model.resources.end()) {
    return res->second.ctype;
  }
  auto it = model.typeMap.find(type);
  if (it != model.typeMap.end()) {
    return it->second;
  }
  if (type == "string") {
    return "const char*";
  }
  return type;
}

std::string escapeCString(const std::string &s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c;
    }
  }
  return out;
}

bool isVoidPointer(const std::string &cType) {
  return cType.find("void") != std::string::npos;
}

bool isStringPointer(const std::string &cType) {
  return cType.find("char") != std::string::npos;
}

std::string observeOf(const model::Model &model, const std::string &type) {
  auto it = model.resources.find(type);
  if (it != model.resources.end()) {
    return it->second.observe;
  }
  return {};
}

std::string operandExpr(const model::Model &model, const model::Function &fn,
                        const std::map<std::string, std::string> &paramExpr,
                        const std::string &returnVar, const std::string &operand) {
  if (operand == "result") {
    return returnVar;
  }
  if (operand == "NULL") {
    return "NULL";
  }
  auto it = paramExpr.find(operand);
  if (it != paramExpr.end()) {
    return it->second;
  }
  return operand;
}

}  // namespace

std::string generate(const model::Model &model, const std::vector<gen::Sequence> &sequences) {
  std::ostringstream out;
  out << "#include <stddef.h>\n";
  out << "#include <stdio.h>\n";
  out << "#include <string.h>\n\n";

  for (const auto &[name, resource] : model.resources) {
    (void)name;
    if (!resource.observe.empty()) {
      out << "const char* " << resource.observe << "(" << resource.ctype << ");\n";
    }
  }
  for (const auto &function : model.functions) {
    if (!function.signature.empty()) {
      out << function.signature << ";\n";
    }
  }
  out << "\n";

  for (std::size_t s = 0; s < sequences.size(); ++s) {
    const gen::Sequence &seq = sequences[s];
    out << "static int test_" << s << "(void) {\n";
    out << "  char buf[256] = {0};\n";

    int nextHandle = 0;
    for (std::size_t c = 0; c < seq.calls.size(); ++c) {
      const gen::Call &call = seq.calls[c];
      auto fnIt = std::find_if(model.functions.begin(), model.functions.end(),
                               [&](const model::Function &f) { return f.name == call.function; });
      if (fnIt == model.functions.end()) {
        continue;
      }
      const model::Function &fn = *fnIt;

      std::map<std::string, std::string> paramExpr;
      std::vector<std::string> args;
      std::vector<std::string> decls;
      for (std::size_t i = 0; i < fn.params.size(); ++i) {
        const model::Param &param = fn.params[i];
        std::string expr;
        if (isResource(model, param.type)) {
          expr = "h" + std::to_string(call.resourceArgs[i]);
        } else {
          const std::string cType = cTypeOf(model, param.type);
          if (param.out && isStringPointer(cType)) {
            expr = "out_" + param.name;
            decls.push_back("char " + expr + "[256] = {0};");
          } else if (isVoidPointer(cType)) {
            expr = "buf";
          } else if (isStringPointer(cType)) {
            expr = "\"" + escapeCString(call.values[i]) + "\"";
          } else {
            expr = call.values[i].empty() ? "0" : call.values[i];
          }
        }
        args.push_back(expr);
        paramExpr[param.name] = expr;
      }

      const bool resourceReturn = isResource(model, fn.returnType);
      std::string returnVar;
      std::string callStmt;

      if (resourceReturn) {
        returnVar = "h" + std::to_string(nextHandle++);
        callStmt = cTypeOf(model, fn.returnType) + " " + returnVar + " = " + fn.symbol + "(";
      } else if (!fn.returnType.empty()) {
        returnVar = "r" + std::to_string(c);
        callStmt = cTypeOf(model, fn.returnType) + " " + returnVar + " = " + fn.symbol + "(";
      } else {
        callStmt = fn.symbol + "(";
      }

      for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
          callStmt += ", ";
        }
        callStmt += args[i];
      }
      callStmt += ");";
      for (const auto &decl : decls) {
        out << "  " << decl << "\n";
      }
      out << "  " << callStmt << "\n";

      std::string success;
      if (fn.success.kind == model::SuccessExpr::Kind::True) {
        success = "1";
      } else if (fn.success.kind == model::SuccessExpr::Kind::False) {
        success = "0";
      } else {
        success = operandExpr(model, fn, paramExpr, returnVar, fn.success.lhs) + " " +
                  fn.success.op + " " +
                  operandExpr(model, fn, paramExpr, returnVar, fn.success.rhs);
      }

      out << "  if (!(" << success << ")) { printf(\"FAIL " << s << " " << fn.name
          << "\\n\"); return 1; }\n";

      for (const auto &effect : fn.effects) {
        std::string type;
        std::string handleExpr;
        if (effect.target == "result") {
          type = fn.returnType;
          handleExpr = returnVar;
        } else {
          for (const auto &param : fn.params) {
            if (param.name == effect.target) {
              type = param.type;
              handleExpr = paramExpr[param.name];
              break;
            }
          }
        }
        const std::string observe = observeOf(model, type);
        if (!observe.empty() && !handleExpr.empty()) {
          out << "  if (strcmp(" << observe << "(" << handleExpr << "), \"" << effect.state
              << "\") != 0) { printf(\"FAIL " << s << " " << fn.name << " state\\n\"); return 1; }\n";
        }
      }
    }

    out << "  return 0;\n";
    out << "}\n\n";
  }

  out << "int main(void) {\n";
  out << "  int failed = 0;\n";
  for (std::size_t s = 0; s < sequences.size(); ++s) {
    out << "  failed += test_" << s << "();\n";
  }
  out << "  if (failed) { printf(\"FAILED %d\\n\", failed); return 1; }\n";
  out << "  printf(\"ALL PASS\\n\");\n";
  out << "  return 0;\n";
  out << "}\n";

  return out.str();
}

}  // namespace harness
