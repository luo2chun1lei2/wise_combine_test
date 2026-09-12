#pragma once

#include <map>
#include <string>
#include <vector>

namespace model {

struct ValueSource {
  std::string name;
  bool isRange = false;
  std::vector<std::string> items;
  int lo = 0;
  int hi = 0;
};

struct Resource {
  std::string name;
  std::string ctype;
  std::vector<std::string> states;
  std::string initial;
  std::string observe;
  std::map<std::string, int> intVars;
  std::map<std::string, std::vector<int>> listVars;
};

struct Param {
  std::string name;
  std::string type;
  std::string valueSource;
  bool out = false;
};

struct Cond {
  std::string param;
  std::string state;
};

struct Effect {
  std::string target;
  std::string state;
};

struct SuccessExpr {
  std::string expr;
};

struct Update {
  enum Kind { Set, Add, Sub, Append, PopFront };
  Kind kind = Set;
  std::string target;
  int value = 0;
  std::string valueName;
};

struct Function {
  std::string name;
  std::vector<Param> params;
  std::string returnType;
  std::string symbol;
  std::string signature;
  std::string receiver;
  std::vector<Cond> requiresConds;
  std::vector<Effect> effects;
  std::vector<Update> updates;
  SuccessExpr success;
};

struct ClassEntry {
  std::string name;
  std::string cpp;
  std::string header;
};

struct SetupEntry {
  std::string name;
  std::string function;
  std::vector<std::string> args;
  int count = 0;
};

struct Model {
  std::map<std::string, std::string> typeMap;
  std::map<std::string, ValueSource> values;
  std::map<std::string, Resource> resources;
  std::vector<Function> functions;
  std::map<std::string, ClassEntry> classes;
  std::vector<SetupEntry> setups;
  std::vector<std::string> errors;
};

}  // namespace model
