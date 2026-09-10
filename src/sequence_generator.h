#pragma once

#include <functional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "model.h"

namespace gen {

struct Call {
  std::string function;
  std::vector<int> resourceArgs;  // 实例编号；值参数用 -1 占位
  std::vector<std::string> values;  // 值参数的具体取值；资源参数为空
};

struct Sequence {
  std::vector<Call> calls;
  std::string text() const;
};

class SequenceGenerator {
 public:
  SequenceGenerator(const model::Model &model, int maxLength, unsigned seed = 0,
                    bool negative = false, int maxCases = 0);
  std::vector<Sequence> generate();
  const std::vector<Sequence> &negativeSequences() const;

 private:
  struct Instance {
    std::string type;
    std::string state;
    int id = -1;
  };

  const model::Model &model_;
  int maxLength_ = 0;
  bool negative_ = false;
  int maxCases_ = 0;
  std::mt19937 rng_;
  std::vector<Sequence> results_;
  std::set<std::string> seen_;
  std::vector<Sequence> negativeResults_;
  std::set<std::string> negativeSeen_;

  void dfs(std::vector<Instance> &env, Sequence &seq, int nextId);
  std::string sampleValue(const model::ValueSource &source);
  bool hasAnyBinding(const model::Function &fn, const std::vector<Instance> &env,
                     std::size_t paramIndex, std::vector<bool> &used) const;
  bool reachedLimit() const;
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
