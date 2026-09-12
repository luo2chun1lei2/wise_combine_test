#include "sequence_generator.h"

#include <algorithm>
#include <deque>
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
  int nextId = 0;
  applySetups(env, nextId);
  Sequence seq;
  dfs(env, seq, nextId);
  return results_;
}

std::vector<Sequence> SequenceGenerator::generateBfs() {
  struct Node {
    std::vector<Instance> env;
    Sequence seq;
    int nextId = 0;
  };

  results_.clear();
  seen_.clear();
  std::deque<Node> queue;
  std::vector<Instance> initialEnv;
  int initialNextId = 0;
  applySetups(initialEnv, initialNextId);
  queue.push_back({initialEnv, {}, initialNextId});

  while (!queue.empty()) {
    if (reachedLimit()) {
      break;
    }
    Node node = queue.front();
    queue.pop_front();
    if (static_cast<int>(node.seq.calls.size()) >= maxLength_) {
      continue;
    }

    bool stop = false;
    for (const auto &function : model_.functions) {
      std::vector<int> bindings(function.params.size(), -1);
      std::vector<bool> used(node.env.size(), false);
      enumerateBindings(function, node.env, 0, bindings, used,
                        [&](const std::vector<int> &bound) {
        Sequence next = node.seq;
        Call call;
        call.function = function.name;
        for (std::size_t i = 0; i < function.params.size(); ++i) {
          if (bound[i] >= 0) {
            call.resourceArgs.push_back(node.env[bound[i]].id);
            call.values.push_back("");
          } else {
            call.resourceArgs.push_back(-1);
            const std::string &sourceName = function.params[i].valueSource;
            auto it = model_.values.find(sourceName);
            call.values.push_back(it != model_.values.end() ? sampleValue(it->second) : "");
          }
        }
        next.calls.push_back(call);

        std::vector<Instance> newEnv = node.env;
        int newNextId = node.nextId;
        apply(function, bound, newEnv, newNextId);

        if (seen_.insert(next.text()).second) {
          results_.push_back(next);
          queue.push_back({newEnv, next, newNextId});
          if (reachedLimit()) {
            stop = true;
          }
        }
      });
      if (stop) {
        break;
      }
    }
  }
  return results_;
}

const std::vector<Sequence> &SequenceGenerator::negativeSequences() const {
  return negativeResults_;
}

void SequenceGenerator::setBindRandom(bool value) {
  bindRandom_ = value;
}

std::vector<Sequence> SequenceGenerator::generateRandom(unsigned seed, int count) {
  std::vector<Sequence> out;
  std::set<std::string> seen;
  std::mt19937 rng(seed);
  const int attempts = (count <= 0) ? 20 : count;

  for (int attempt = 0; attempt < attempts; ++attempt) {
    std::vector<Instance> env;
    Sequence seq;
    int nextId = 0;
    applySetups(env, nextId);

    while (static_cast<int>(seq.calls.size()) < maxLength_) {
      std::vector<const model::Function *> applicable;
      for (const auto &function : model_.functions) {
        std::vector<bool> used(env.size(), false);
        if (hasAnyBinding(function, env, 0, used)) {
          applicable.push_back(&function);
        }
      }
      if (applicable.empty()) {
        break;
      }

      const model::Function *fn = applicable[rng() % applicable.size()];
      std::vector<int> bindings(fn->params.size(), -1);
      std::vector<bool> used(env.size(), false);
      std::vector<std::vector<int>> candidates;
      enumerateBindings(*fn, env, 0, bindings, used,
                        [&](const std::vector<int> &bound) { candidates.push_back(bound); });
      if (candidates.empty()) {
        break;
      }
      const std::vector<int> &bound = candidates[rng() % candidates.size()];

      Call call;
      call.function = fn->name;
      for (std::size_t i = 0; i < fn->params.size(); ++i) {
        if (bound[i] >= 0) {
          call.resourceArgs.push_back(env[bound[i]].id);
          call.values.push_back("");
        } else {
          call.resourceArgs.push_back(-1);
          const std::string &sourceName = fn->params[i].valueSource;
          auto it = model_.values.find(sourceName);
          if (it != model_.values.end()) {
            call.values.push_back(sampleValue(it->second));
          } else {
            call.values.push_back("");
          }
        }
      }

      seq.calls.push_back(call);
      apply(*fn, bound, env, nextId);
    }

    if (!seq.calls.empty() && seen.insert(seq.text()).second) {
      out.push_back(seq);
    }
  }
  return out;
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
    std::vector<std::vector<int>> candidates;
    enumerateBindings(function, env, 0, bindings, used,
                      [&](const std::vector<int> &bound) { candidates.push_back(bound); });

    std::size_t start = 0;
    std::size_t end = candidates.size();
    if (bindRandom_ && !candidates.empty()) {
      const std::size_t pick = rng_() % candidates.size();
      start = pick;
      end = pick + 1;
    }

    for (std::size_t k = start; k < end; ++k) {
      const std::vector<int> &bound = candidates[k];
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
    }
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

void SequenceGenerator::applySetups(std::vector<Instance> &env, int &nextId) {
  for (const auto &setup : model_.setups) {
    auto it = std::find_if(model_.functions.begin(), model_.functions.end(),
                           [&](const model::Function &fn) { return fn.name == setup.function; });
    if (it == model_.functions.end()) {
      continue;
    }
    std::vector<int> bindings(it->params.size(), -1);
    for (int i = 0; i < setup.count; ++i) {
      apply(*it, bindings, env, nextId);
    }
  }
}

}  // namespace gen
