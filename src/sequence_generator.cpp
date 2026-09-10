#include "sequence_generator.h"

#include <sstream>

namespace gen {

namespace {

bool isResourceType(const model::Model &model, const std::string &type) {
  return model.resources.find(type) != model.resources.end();
}

std::string resourceStateFor(const model::Model &model, const std::string &type,
                             const std::string &state) {
  if (!state.empty()) {
    return state;
  }
  auto it = model.resources.find(type);
  if (it != model.resources.end()) {
    return it->second.initial;
  }
  return {};
}

}  // namespace

std::string Sequence::text() const {
  std::string out;
  for (std::size_t i = 0; i < calls.size(); ++i) {
    if (i > 0) {
      out += " ; ";
    }
    out += calls[i].function;
    out += "(";
    for (std::size_t j = 0; j < calls[i].resourceArgs.size(); ++j) {
      if (j > 0) {
        out += ", ";
      }
      if (calls[i].resourceArgs[j] >= 0) {
        out += std::to_string(calls[i].resourceArgs[j]);
      } else if (!calls[i].values[j].empty()) {
        out += calls[i].values[j];
      } else {
        out += "_";
      }
    }
    out += ")";
  }
  return out;
}

SequenceGenerator::SequenceGenerator(const model::Model &model, int maxLength, unsigned seed,
                                    bool negative, int maxCases)
    : model_(model),
      maxLength_(maxLength),
      negative_(negative),
      maxCases_(maxCases),
      rng_(seed) {}

std::vector<Sequence> SequenceGenerator::generate() {
  results_.clear();
  seen_.clear();
  negativeResults_.clear();
  negativeSeen_.clear();

  std::vector<Instance> env;
  Sequence seq;
  dfs(env, seq, 0);
  return results_;
}

const std::vector<Sequence> &SequenceGenerator::negativeSequences() const {
  return negativeResults_;
}

void SequenceGenerator::dfs(std::vector<Instance> &env, Sequence &seq, int nextId) {
  if (reachedLimit() || static_cast<int>(seq.calls.size()) >= maxLength_) {
    return;
  }

  for (const auto &function : model_.functions) {
    if (reachedLimit()) {
      return;
    }

    std::vector<bool> used(env.size(), false);
    const bool applicable = hasAnyBinding(function, env, 0, used);

    if (negative_ && !applicable) {
      Call call;
      call.function = function.name;
      for (std::size_t i = 0; i < function.params.size(); ++i) {
        call.resourceArgs.push_back(-1);
        const std::string &sourceName = function.params[i].valueSource;
        auto it = model_.values.find(sourceName);
        if (it != model_.values.end()) {
          call.values.push_back(sampleValue(it->second));
        } else {
          call.values.push_back("");
        }
      }
      Sequence negative = seq;
      negative.calls.push_back(call);
      if (negativeSeen_.insert(negative.text()).second) {
        negativeResults_.push_back(negative);
      }
      continue;
    }

    std::vector<int> bindings(function.params.size(), -1);
    used.assign(env.size(), false);
    enumerateBindings(function, env, 0, bindings, used, [&](const std::vector<int> &bound) {
      Sequence next = seq;
      Call call;
      call.function = function.name;
      for (std::size_t i = 0; i < function.params.size(); ++i) {
        if (bound[i] >= 0) {
          call.resourceArgs.push_back(env[bound[i]].id);
          call.values.push_back("");
        } else {
          call.resourceArgs.push_back(-1);
          const std::string &sourceName = function.params[i].valueSource;
          auto it = model_.values.find(sourceName);
          if (it != model_.values.end()) {
            call.values.push_back(sampleValue(it->second));
          } else {
            call.values.push_back("");
          }
        }
      }
      next.calls.push_back(call);

      std::vector<Instance> newEnv = env;
      int newNextId = nextId;
      apply(function, bound, newEnv, newNextId);

      const std::string key = next.text();
      if (seen_.insert(key).second) {
        results_.push_back(next);
        dfs(newEnv, next, newNextId);
      }
    });
  }
}

bool SequenceGenerator::hasAnyBinding(const model::Function &fn, const std::vector<Instance> &env,
                                      std::size_t paramIndex, std::vector<bool> &used) const {
  if (paramIndex == fn.params.size()) {
    return true;
  }

  const model::Param &param = fn.params[paramIndex];
  if (!isResourceType(model_, param.type)) {
    return hasAnyBinding(fn, env, paramIndex + 1, used);
  }

  for (std::size_t j = 0; j < env.size(); ++j) {
    if (used[j] || env[j].type != param.type) {
      continue;
    }

    bool ok = true;
    for (const auto &cond : fn.requiresConds) {
      if (cond.param == param.name && env[j].state != cond.state) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      continue;
    }

    used[j] = true;
    if (hasAnyBinding(fn, env, paramIndex + 1, used)) {
      return true;
    }
    used[j] = false;
  }
  return false;
}

bool SequenceGenerator::reachedLimit() const {
  return maxCases_ > 0 &&
         static_cast<int>(results_.size() + negativeResults_.size()) >= maxCases_;
}

std::string SequenceGenerator::sampleValue(const model::ValueSource &source) {
  if (source.isRange) {
    const int span = source.hi - source.lo + 1;
    if (span <= 0) {
      return std::to_string(source.lo);
    }
    return std::to_string(source.lo + static_cast<int>(rng_() % static_cast<unsigned>(span)));
  }
  if (source.items.empty()) {
    return {};
  }
  return source.items[rng_() % source.items.size()];
}

void SequenceGenerator::enumerateBindings(
    const model::Function &fn, const std::vector<Instance> &env, std::size_t paramIndex,
    std::vector<int> &bindings, std::vector<bool> &used,
    const std::function<void(const std::vector<int> &)> &emit) {
  if (paramIndex == fn.params.size()) {
    emit(bindings);
    return;
  }

  const model::Param &param = fn.params[paramIndex];
  if (!isResourceType(model_, param.type)) {
    bindings[paramIndex] = -1;
    enumerateBindings(fn, env, paramIndex + 1, bindings, used, emit);
    return;
  }

  for (std::size_t j = 0; j < env.size(); ++j) {
    if (used[j] || env[j].type != param.type) {
      continue;
    }

    bool ok = true;
    for (const auto &cond : fn.requiresConds) {
      if (cond.param == param.name && env[j].state != cond.state) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      continue;
    }

    bindings[paramIndex] = static_cast<int>(j);
    used[j] = true;
    enumerateBindings(fn, env, paramIndex + 1, bindings, used, emit);
    used[j] = false;
    bindings[paramIndex] = -1;
  }
}

bool SequenceGenerator::satisfies(const model::Function &fn, const std::vector<int> &bindings,
                                  const std::vector<Instance> &env) const {
  for (const auto &cond : fn.requiresConds) {
    int paramIndex = -1;
    for (std::size_t i = 0; i < fn.params.size(); ++i) {
      if (fn.params[i].name == cond.param) {
        paramIndex = static_cast<int>(i);
        break;
      }
    }
    if (paramIndex < 0 || bindings[paramIndex] < 0 ||
        bindings[paramIndex] >= static_cast<int>(env.size()) ||
        env[bindings[paramIndex]].state != cond.state) {
      return false;
    }
  }
  return true;
}

void SequenceGenerator::apply(const model::Function &fn, const std::vector<int> &bindings,
                              std::vector<Instance> &env, int &nextId) {
  if (isResourceType(model_, fn.returnType)) {
    std::string resultState = resourceStateFor(model_, fn.returnType, {});
    for (const auto &effect : fn.effects) {
      if (effect.target == "result") {
        resultState = effect.state;
      }
    }
    Instance instance;
    instance.type = fn.returnType;
    instance.state = resultState;
    instance.id = nextId++;
    env.push_back(instance);
  }

  for (const auto &effect : fn.effects) {
    if (effect.target == "result") {
      continue;
    }
    int paramIndex = -1;
    for (std::size_t i = 0; i < fn.params.size(); ++i) {
      if (fn.params[i].name == effect.target) {
        paramIndex = static_cast<int>(i);
        break;
      }
    }
    if (paramIndex >= 0 && bindings[paramIndex] >= 0 &&
        bindings[paramIndex] < static_cast<int>(env.size())) {
      env[bindings[paramIndex]].state = effect.state;
    }
  }
}

}  // namespace gen
