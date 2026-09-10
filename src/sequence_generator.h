#pragma once

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "model.h"

namespace gen {

struct Call {
  std::string function;
  std::vector<int> resourceArgs;  // 实例编号；值参数用 -1 占位
};

struct Sequence {
  std::vector<Call> calls;
  std::string text() const;
};

class SequenceGenerator {
 public:
  SequenceGenerator(const model::Model &model, int maxLength);
  std::vector<Sequence> generate();

 private:
  struct Instance {
    std::string type;
    std::string state;
    int id = -1;
  };

  const model::Model &model_;
  int maxLength_ = 0;
  std::vector<Sequence> results_;
  std::set<std::string> seen_;

  void dfs(std::vector<Instance> &env, Sequence &seq, int nextId);
  bool satisfies(const model::Function &fn, const std::vector<int> &bindings,
                 const std::vector<Instance> &env) const;
  void apply(const model::Function &fn, const std::vector<int> &bindings,
             std::vector<Instance> &env, int &nextId);
  void enumerateBindings(const model::Function &fn, const std::vector<Instance> &env,
                         std::size_t paramIndex, std::vector<int> &bindings,
                         std::vector<bool> &used,
                         const std::function<void(const std::vector<int> &)> &emit);
};

}  // namespace gen
