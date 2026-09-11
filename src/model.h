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

struct Function {
  std::string name;
  std::vector<Param> params;
  std::string returnType;
  std::string symbol;
  std::string signature;
  std::vector<Cond> requiresConds;
  std::vector<Effect> effects;
  SuccessExpr success;
};

struct Model {
  std::map<std::string, std::string> typeMap;
  std::map<std::string, ValueSource> values;
  std::map<std::string, Resource> resources;
  std::vector<Function> functions;
  std::vector<std::string> errors;
};

}  // namespace model
