#include "harness_generator.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

#include "guard.h"

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

struct ActualValue {
  std::string format;
  std::string expr;
};

ActualValue actualValue(const model::Model &model, const std::string &type,
                        const std::string &expr) {
  const std::string ctype = cTypeOf(model, type);
  if (ctype.find("char") != std::string::npos && ctype.find("*") != std::string::npos) {
    return {"%s", expr};
  }
  if (ctype.find("*") != std::string::npos) {
    return {"%p", "(void*)(" + expr + ")"};
  }
  if (ctype == "size_t" || ctype == "unsigned long" || ctype == "unsigned long long") {
    return {"%zu", expr};
  }
  if (ctype == "long" || ctype == "long long") {
    return {"%lld", "(long long)(" + expr + ")"};
  }
  return {"%d", "(int)(" + expr + ")"};
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

std::string translateSuccess(const model::Model &model,
                             const std::map<std::string, std::string> &paramExpr,
                             const std::string &returnVar,
                             const std::map<std::string, std::string> &stateExpr,
                             const std::map<std::string, std::string> &listExpr,
                             const std::string &expr) {
  (void)model;
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
      } else if (token == "len" || token == "front") {
        std::size_t k = j;
        while (k < expr.size() && std::isspace(static_cast<unsigned char>(expr[k]))) {
          ++k;
        }
        if (k < expr.size() && expr[k] == '(') {
          ++k;
          while (k < expr.size() && std::isspace(static_cast<unsigned char>(expr[k]))) {
            ++k;
          }
          std::size_t start = k;
          while (k < expr.size() &&
                 (std::isalnum(static_cast<unsigned char>(expr[k])) || expr[k] == '_')) {
            ++k;
          }
          const std::string listName = expr.substr(start, k - start);
          while (k < expr.size() && std::isspace(static_cast<unsigned char>(expr[k]))) {
            ++k;
          }
          if (k < expr.size() && expr[k] == ')') {
            const auto it = listExpr.find(listName);
            if (it != listExpr.end()) {
              out += (token == "len") ? (it->second + "_len") : (it->second + "[0]");
              i = k + 1;
              continue;
            }
          }
        }
        out += token;
      } else if (token == "true") {
        out += "1";
      } else if (token == "false") {
        out += "0";
      } else if (token == "NULL") {
        out += "NULL";
      } else {
        auto stateIt = stateExpr.find(token);
        auto paramIt = paramExpr.find(token);
        if (stateIt != stateExpr.end()) {
          out += stateIt->second;
        } else if (paramIt != paramExpr.end()) {
          out += paramIt->second;
        } else {
          out += token;
        }
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

std::string isolationIncludes() {
  return "#include <sys/types.h>\n"
         "#include <sys/wait.h>\n"
         "#include <unistd.h>\n"
         "#include <signal.h>\n"
         "#include <time.h>\n";
}

std::string featureMacro() {
  return "#ifndef _DEFAULT_SOURCE\n"
         "#define _DEFAULT_SOURCE 1\n"
         "#endif\n";
}

std::string runTestHelper() {
  return "\n"
         "static int run_test(int timeout_seconds, int (*fn)(void)) {\n"
         "  pid_t pid = fork();\n"
         "  if (pid < 0) { return 1; }\n"
         "  if (pid == 0) {\n"
         "    setpgid(0, 0);\n"
         "    int rc = fn();\n"
         "    fflush(stdout);\n"
         "    fflush(stderr);\n"
         "    _exit(rc == 0 ? 0 : 1);\n"
         "  }\n"
         "  setpgid(pid, pid);\n"
         "  int status = 0;\n"
         "  if (timeout_seconds <= 0) {\n"
         "    waitpid(pid, &status, 0);\n"
         "  } else {\n"
         "    const time_t deadline = time(NULL) + (time_t)timeout_seconds;\n"
         "    while (1) {\n"
         "      pid_t r = waitpid(pid, &status, WNOHANG);\n"
         "      if (r == pid) { break; }\n"
         "      if (time(NULL) >= deadline) {\n"
         "        kill(-pid, SIGKILL);\n"
         "        waitpid(pid, &status, 0);\n"
         "        return 2;\n"
         "      }\n"
         "      usleep(10000);\n"
         "    }\n"
         "  }\n"
         "  if (WIFEXITED(status)) { return WEXITSTATUS(status) == 0 ? 0 : 1; }\n"
         "  return 1;\n"
         "}\n";
}

}  // namespace

std::string generate(const model::Model &model, const std::vector<gen::Sequence> &sequences,
                     bool dylib, bool jsonFailures, int timeoutSeconds) {
  std::ostringstream out;
  out << featureMacro();
  out << "#include <stddef.h>\n";
  out << "#include <stdio.h>\n";
  out << "#include <string.h>\n";
  out << isolationIncludes();
  for (const auto &[name, cls] : model.classes) {
    (void)name;
    if (!cls.header.empty()) {
      out << "#include \"" << cls.header << "\"\n";
    }
  }
  if (dylib) {
    out << "#include <dlfcn.h>\n";
  }
  out << runTestHelper();
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
    bool needBuffer = false;
    for (const auto &call : seq.calls) {
      const auto fnIt = std::find_if(model.functions.begin(), model.functions.end(),
                                     [&](const model::Function &f) {
                                       return f.name == call.function;
                                     });
      if (fnIt == model.functions.end()) {
        continue;
      }
      for (const auto &param : fnIt->params) {
        if (isVoidPointer(cTypeOf(model, param.type))) {
          needBuffer = true;
          break;
        }
      }
      if (needBuffer) {
        break;
      }
    }
    out << "static int test_" << s << "(void) {\n";
    if (needBuffer) {
      out << "  char buf[256] = {0};\n";
    }
    for (const auto &[name, cls] : model.classes) {
      (void)name;
      out << "  " << cls.cpp << " obj_" << cls.name << ";\n";
    }

    std::map<std::string, std::string> stateExpr;
    std::map<std::string, std::string> listExpr;
    for (const auto &[resourceName, resource] : model.resources) {
      for (const auto &[varName, initial] : resource.intVars) {
        const std::string cvar = "shadow_" + resourceName + "_" + varName;
        out << "  int " << cvar << " = " << initial << ";\n";
        stateExpr[varName] = cvar;
      }
      for (const auto &[varName, initial] : resource.listVars) {
        const std::string base = "shadow_" + resourceName + "_" + varName;
        out << "  int " << base << "[64];\n";
        out << "  int " << base << "_len = 0;\n";
        for (std::size_t idx = 0; idx < initial.size(); ++idx) {
          out << "  " << base << "[" << idx << "] = " << initial[idx] << ";\n";
        }
        out << "  " << base << "_len = " << initial.size() << ";\n";
        listExpr[varName] = base;
      }
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

      const std::string success =
          translateSuccess(model, paramExpr, returnVar, stateExpr, listExpr, fn.success.expr);
      const bool hasSuccessActual = !fn.returnType.empty() && !returnVar.empty();
      const ActualValue successActual =
          hasSuccessActual ? actualValue(model, fn.returnType, returnVar) : ActualValue{};

      if (jsonFailures) {
        out << "  if (!(" << success << ")) { printf(\"{\\\"kind\\\":\\\"failure\\\",\\\"seq\\\":"
            << s << ",\\\"step\\\":\\\"" << fn.name << "\\\",\\\"expected\\\":\\\""
            << escapeCString(fn.success.expr) << "\\\"";
        if (hasSuccessActual) {
          out << ",\\\"actual\\\":\\\"" << successActual.format << "\\\"";
        }
        out << "}\\n\"";
        if (hasSuccessActual) {
          out << ", " << successActual.expr;
        }
        out << "); return 1; }\n";
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
                << "\\\",\\\"actual\\\":\\\"%s\\\"}\\n\", " << observeFn << "(" << handleExpr
                << ")); return 1; }\n";
          } else {
            out << "  if (strcmp(" << observeFn << "(" << handleExpr << "), \"" << effect.state
                << "\") != 0) { printf(\"FAIL " << s << " " << fn.name << " state\\n\"); return 1; }\n";
          }
        }
      }

      for (const auto &update : fn.updates) {
        std::string valueExpr;
        if (update.kind == model::Update::Set || update.kind == model::Update::Add ||
            update.kind == model::Update::Sub || update.kind == model::Update::Append) {
          if (update.valueName.empty()) {
            valueExpr = std::to_string(update.value);
          } else if (update.valueName == "result") {
            valueExpr = returnVar;
          } else {
            valueExpr = paramExpr[update.valueName];
          }
        }

        if (update.kind == model::Update::PopFront) {
          const auto listIt = listExpr.find(update.target);
          if (listIt != listExpr.end()) {
            out << "  for (int _i = 1; _i < " << listIt->second << "_len; ++_i) { "
                << listIt->second << "[_i - 1] = " << listIt->second << "[_i]; }\n";
            out << "  if (" << listIt->second << "_len > 0) { " << listIt->second
                << "_len -= 1; }\n";
          }
        } else if (update.kind == model::Update::Append) {
          const auto listIt = listExpr.find(update.target);
          if (listIt != listExpr.end()) {
            out << "  " << listIt->second << "[" << listIt->second << "_len++] = " << valueExpr
                << ";\n";
          }
        } else {
          const auto stateIt = stateExpr.find(update.target);
          if (stateIt != stateExpr.end()) {
            const char *op = update.kind == model::Update::Add
                                 ? " += "
                                 : (update.kind == model::Update::Sub ? " -= " : " = ");
            out << "  " << stateIt->second << op << valueExpr << ";\n";
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

  if (!sequences.empty()) {
    out << "  static int (*const tests[])(void) = {";
    for (std::size_t s = 0; s < sequences.size(); ++s) {
      if (s > 0) {
        out << ", ";
      }
      out << "test_" << s;
    }
    out << "};\n";
  }

  out << "  int failed = 0;\n";
  for (std::size_t s = 0; s < sequences.size(); ++s) {
    out << "  {\n";
    out << "    int rc = run_test(" << timeoutSeconds << ", tests[" << s << "]);\n";
    out << "    if (rc == 2) { printf(\"TIMEOUT " << s << "\\n\"); failed++; }\n";
    out << "    else { failed += rc; }\n";
    out << "  }\n";
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

std::string generateStateMachine(const smodel::StateMachine &machine,
                                 const std::vector<std::string> &events,
                                 const std::map<std::string, std::string> &guardValues,
                                 int timeoutSeconds) {
  std::ostringstream out;
  out << featureMacro();
  out << "#include <cstdio>\n";
  out << "#include <cstring>\n";
  out << isolationIncludes();
  for (const auto &[name, cls] : machine.classes) {
    (void)name;
    if (!cls.header.empty()) {
      out << "#include \"" << cls.header << "\"\n";
    }
  }
  out << runTestHelper();
  out << "\n";

  for (const auto &[name, cls] : machine.classes) {
    (void)name;
    out << cls.cpp << " obj_" << cls.name << ";\n";
  }
  out << "\n";

  out << "static void run_action(const char* name) {\n";
  for (const auto &[actionName, action] : machine.actions) {
    out << "  if (std::strcmp(name, \"" << actionName << "\") == 0) { obj_" << action.className
        << "." << action.method << "(); return; }\n";
  }
  out << "}\n\n";

  out << "static int run_sm(void) {\n";
  out << "  const char* current = \"" << smodel::leafOf(machine, machine.initial) << "\";\n";
  out << "  const char* events[] = {";
  for (std::size_t i = 0; i < events.size(); ++i) {
    if (i > 0) out << ", ";
    out << "\"" << escapeCString(events[i]) << "\"";
  }
  out << "};\n";
  out << "  const int n = " << events.size() << ";\n";
  out << "  for (int i = 0; i < n; ++i) {\n";
  out << "    const char* e = events[i];\n";
  out << "    const char* next = 0;\n";

  for (const auto &transition : machine.transitions) {
    std::string guardError;
    if (!transition.guard.empty() &&
        !guard::evalGuard(transition.guard, guardValues, &guardError)) {
      continue;
    }

    std::vector<std::string> fireableLeaves;
    for (const auto &state : machine.states) {
      const auto *info = smodel::findState(machine, state);
      if (info == nullptr || !info->children.empty()) {
        continue;
      }
      if (state == transition.from ||
          smodel::isDescendantOf(machine, state, transition.from)) {
        fireableLeaves.push_back(state);
      }
    }
    if (fireableLeaves.empty()) {
      fireableLeaves.push_back(transition.from);
    }

    out << "    if ((";
    for (std::size_t i = 0; i < fireableLeaves.size(); ++i) {
      if (i > 0) {
        out << " || ";
      }
      out << "std::strcmp(current, \"" << escapeCString(fireableLeaves[i]) << "\") == 0";
    }
    out << ") && std::strcmp(e, \"" << escapeCString(transition.event)
        << "\") == 0) {\n";
    auto emitAction = [&](const std::string &name) {
      if (!name.empty()) {
        out << "      run_action(\"" << escapeCString(name) << "\");\n";
      }
    };
    const auto *fromInfo = smodel::findState(machine, transition.from);
    if (fromInfo != nullptr) {
      emitAction(fromInfo->exit);
    }
    emitAction(transition.action);
    const std::string targetLeaf = smodel::leafOf(machine, transition.to);
    const auto *toInfo = smodel::findState(machine, targetLeaf);
    if (toInfo != nullptr) {
      emitAction(toInfo->entry);
    }
    out << "      next = \"" << targetLeaf << "\";\n";
    out << "    }\n";
  }

  out << "    if (!next) { std::printf(\"FAIL: no transition for %s in %s\\n\", e, current); return 1; }\n";
  out << "    current = next;\n";
  out << "  }\n";
  out << "  std::printf(\"ALL PASS\\n\");\n";
  out << "  return 0;\n";
  out << "}\n";
  out << "\n";
  out << "int main() {\n";
  out << "  int rc = run_test(" << timeoutSeconds << ", run_sm);\n";
  out << "  if (rc == 2) { std::printf(\"TIMEOUT\\n\"); return 1; }\n";
  out << "  return rc;\n";
  out << "}\n";
  return out.str();
}

}  // namespace harness
