#include "harness_generator.h"

#include <algorithm>
#include <cctype>
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

struct SigParts {
  std::string ret;
  std::string params;
};

SigParts parseSig(const std::string &signature) {
  const std::size_t paren = signature.find('(');
  std::string left = signature.substr(0, paren);
  const std::string params = signature.substr(paren);
  while (!left.empty() && (left.back() == ' ' || left.back() == '\t')) {
    left.pop_back();
  }
  const std::size_t space = left.find_last_of(" \t");
  std::string ret = (space == std::string::npos) ? "" : left.substr(0, space);
  return {ret, params};
}

void emitSetups(std::ostringstream &out, const model::Model &model, int &nextHandle, bool dylib) {
  for (const auto &setup : model.setups) {
    auto it = std::find_if(model.functions.begin(), model.functions.end(),
                           [&](const model::Function &fn) { return fn.name == setup.function; });
    if (it == model.functions.end()) {
      continue;
    }
    const std::string ctype = cTypeOf(model, it->returnType);
    const std::string symbol = dylib ? it->symbol + "_p" : it->symbol;
    for (int k = 0; k < setup.count; ++k) {
      out << "  " << ctype << " h" << nextHandle << " = " << symbol << "(";
      for (std::size_t i = 0; i < setup.args.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        bool numeric = !setup.args[i].empty();
        for (char ch : setup.args[i]) {
          if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '-') {
            numeric = false;
            break;
          }
        }
        out << (numeric ? setup.args[i] : "\"" + escapeCString(setup.args[i]) + "\"");
      }
      out << ");\n";
      ++nextHandle;
    }
  }
}

std::string translateSuccess(const std::map<std::string, std::string> &paramExpr,
                             const std::string &returnVar, const std::string &expr) {
  std::string out;
  for (std::size_t i = 0; i < expr.size();) {
    const char c = expr[i];
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::size_t j = i;
      while (j < expr.size() &&
             (std::isalnum(static_cast<unsigned char>(expr[j])) || expr[j] == '_')) {
        ++j;
      }
      const std::string token = expr.substr(i, j - i);
      if (token == "result") {
        out += returnVar;
      } else if (token == "true") {
        out += "1";
      } else if (token == "false") {
        out += "0";
      } else if (token == "NULL") {
        out += "NULL";
      } else {
        auto it = paramExpr.find(token);
        out += (it != paramExpr.end()) ? it->second : token;
      }
      i = j;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t j = i;
      while (j < expr.size() && std::isdigit(static_cast<unsigned char>(expr[j]))) {
        ++j;
      }
      out += expr.substr(i, j - i);
      i = j;
    } else if (c == '=' && i + 1 < expr.size() && expr[i + 1] == '=') {
      out += "==";
      i += 2;
    } else if (c == '!' && i + 1 < expr.size() && expr[i + 1] == '=') {
      out += "!=";
      i += 2;
    } else if (c == '>' && i + 1 < expr.size() && expr[i + 1] == '=') {
      out += ">=";
      i += 2;
    } else if (c == '<' && i + 1 < expr.size() && expr[i + 1] == '=') {
      out += "<=";
      i += 2;
    } else if (c == '&' && i + 1 < expr.size() && expr[i + 1] == '&') {
      out += "&&";
      i += 2;
    } else if (c == '|' && i + 1 < expr.size() && expr[i + 1] == '|') {
      out += "||";
      i += 2;
    } else {
      out += c;
      ++i;
    }
  }
  return out;
}

}  // namespace

std::string generate(const model::Model &model, const std::vector<gen::Sequence> &sequences,
                     bool dylib, bool jsonFailures) {
  std::ostringstream out;
  out << "#include <stddef.h>\n";
  out << "#include <stdio.h>\n";
  out << "#include <string.h>\n";
  for (const auto &[name, cls] : model.classes) {
    (void)name;
    if (!cls.header.empty()) {
      out << "#include \"" << cls.header << "\"\n";
    }
  }
  if (dylib) {
    out << "#include <dlfcn.h>\n";
  }
  out << "\n";

  if (dylib) {
    for (const auto &[name, resource] : model.resources) {
      (void)name;
      if (!resource.observe.empty()) {
        out << "static const char* (*" << resource.observe << "_p)(" << resource.ctype << ");\n";
      }
    }
    for (const auto &function : model.functions) {
      if (!function.receiver.empty()) {
        continue;
      }
      if (!function.signature.empty()) {
        const SigParts sig = parseSig(function.signature);
        out << "static " << sig.ret << " (*" << function.symbol << "_p)" << sig.params << ";\n";
      }
    }
  } else {
    for (const auto &[name, resource] : model.resources) {
      (void)name;
      if (!resource.observe.empty()) {
        out << "const char* " << resource.observe << "(" << resource.ctype << ");\n";
      }
    }
    for (const auto &function : model.functions) {
      if (!function.receiver.empty()) {
        continue;
      }
      if (!function.signature.empty()) {
        out << function.signature << ";\n";
      }
    }
  }
  out << "\n";

  for (std::size_t s = 0; s < sequences.size(); ++s) {
    const gen::Sequence &seq = sequences[s];
    out << "static int test_" << s << "(void) {\n";
    out << "  char buf[256] = {0};\n";
    for (const auto &[name, cls] : model.classes) {
      (void)name;
      out << "  " << cls.cpp << " obj_" << cls.name << ";\n";
    }

    int nextHandle = 0;
    emitSetups(out, model, nextHandle, dylib);
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
      const std::string callee =
          fn.receiver.empty() ? (dylib ? fn.symbol + "_p" : fn.symbol)
                              : "obj_" + fn.receiver + "." + fn.symbol;

      if (resourceReturn) {
        returnVar = "h" + std::to_string(nextHandle++);
        callStmt = cTypeOf(model, fn.returnType) + " " + returnVar + " = " + callee + "(";
      } else if (!fn.returnType.empty()) {
        returnVar = "r" + std::to_string(c);
        callStmt = cTypeOf(model, fn.returnType) + " " + returnVar + " = " + callee + "(";
      } else {
        callStmt = callee + "(";
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

      const std::string success = translateSuccess(paramExpr, returnVar, fn.success.expr);

      if (jsonFailures) {
        out << "  if (!(" << success << ")) { printf(\"{\\\"kind\\\":\\\"failure\\\",\\\"seq\\\":"
            << s << ",\\\"step\\\":\\\"" << fn.name << "\\\",\\\"expected\\\":\\\""
            << escapeCString(fn.success.expr) << "\\\"}\\n\"); return 1; }\n";
      } else {
        out << "  if (!(" << success << ")) { printf(\"FAIL " << s << " " << fn.name
            << "\\n\"); return 1; }\n";
      }

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
          const std::string observeFn = dylib ? observe + "_p" : observe;
          if (jsonFailures) {
            out << "  if (strcmp(" << observeFn << "(" << handleExpr << "), \"" << effect.state
                << "\") != 0) { printf(\"{\\\"kind\\\":\\\"failure\\\",\\\"seq\\\":" << s
                << ",\\\"step\\\":\\\"" << fn.name << "\\\",\\\"expected\\\":\\\"" << effect.state
                << "\\\"}\\n\"); return 1; }\n";
          } else {
            out << "  if (strcmp(" << observeFn << "(" << handleExpr << "), \"" << effect.state
                << "\") != 0) { printf(\"FAIL " << s << " " << fn.name << " state\\n\"); return 1; }\n";
          }
        }
      }
    }

    out << "  return 0;\n";
    out << "}\n\n";
  }

  if (dylib) {
    out << "int main(int argc, char** argv) {\n";
    out << "  if (argc < 2) { printf(\"usage: %s <lib>\\n\", argv[0]); return 2; }\n";
    out << "  void* lib = dlopen(argv[1], RTLD_LAZY);\n";
    out << "  if (!lib) { printf(\"dlopen: %s\\n\", dlerror()); return 2; }\n";
    for (const auto &[name, resource] : model.resources) {
      (void)name;
      if (!resource.observe.empty()) {
        out << "  *(void**)(&" << resource.observe << "_p) = dlsym(lib, \"" << resource.observe
            << "\");\n";
        out << "  if (!" << resource.observe << "_p) { printf(\"missing " << resource.observe
            << "\\n\"); return 2; }\n";
      }
    }
    for (const auto &function : model.functions) {
      if (!function.receiver.empty()) {
        continue;
      }
      if (!function.signature.empty()) {
        out << "  *(void**)(&" << function.symbol << "_p) = dlsym(lib, \"" << function.symbol
            << "\");\n";
        out << "  if (!" << function.symbol << "_p) { printf(\"missing " << function.symbol
            << "\\n\"); return 2; }\n";
      }
    }
  } else {
    out << "int main(void) {\n";
  }

  out << "  int failed = 0;\n";
  for (std::size_t s = 0; s < sequences.size(); ++s) {
    out << "  failed += test_" << s << "();\n";
  }
  if (dylib) {
    out << "  dlclose(lib);\n";
  }
  out << "  if (failed) { printf(\"FAILED %d\\n\", failed); return 1; }\n";
  out << "  printf(\"ALL PASS\\n\");\n";
  out << "  return 0;\n";
  out << "}\n";

  return out.str();
}

}  // namespace harness
