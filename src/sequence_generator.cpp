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
      out += calls[i].resourceArgs[j] >= 0 ? std::to_string(calls[i].resourceArgs[j]) : "_";
    }
    out += ")";
  }
  return out;
}

SequenceGenerator::SequenceGenerator(const model::Model &model, int maxLength)
    : model_(model), maxLength_(maxLength) {}

std::vector<Sequence> SequenceGenerator::generate() {
  results_.clear();
  seen_.clear();

  std::vector<Instance> env;
  Sequence seq;
  dfs(env, seq, 0);
  return results_;
}

void SequenceGenerator::dfs(std::vector<Instance> &env, Sequence &seq, int nextId) {
  if (static_cast<int>(seq.calls.size()) >= maxLength_) {
    return;
  }

  for (const auto &function : model_.functions) {
    std::vector<int> bindings(function.params.size(), -1);
    std::vector<bool> used(env.size(), false);

    enumerateBindings(function, env, 0, bindings, used, [&](const std::vector<int> &bound) {
      Sequence next = seq;
      Call call;
      call.function = function.name;
      for (std::size_t i = 0; i < function.params.size(); ++i) {
        if (bound[i] >= 0) {
          call.resourceArgs.push_back(env[bound[i]].id);
        } else {
          call.resourceArgs.push_back(-1);
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
