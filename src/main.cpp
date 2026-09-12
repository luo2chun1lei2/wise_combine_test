#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>

#include "FunctionDslLexer.h"
#include "FunctionDslParser.h"
#include "StateMachineDslLexer.h"
#include "StateMachineDslParser.h"
#include "antlr4-runtime.h"
#include "function_model_builder.h"
#include "guard.h"
#include "harness_generator.h"
#include "sequence_generator.h"
#include "state_machine_builder.h"
#include "state_machine_path_generator.h"

namespace {

constexpr const char *kToolVersion = "0.2";

std::string modelHash(const std::string &text) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  char buf[17] = {0};
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(hash));
  return buf;
}

std::string ltrim(const std::string &s) {
  const auto pos = s.find_first_not_of(" \t\r\n");
  return pos == std::string::npos ? "" : s.substr(pos);
}

std::string trim(const std::string &s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

std::string firstKeyword(const std::string &s) {
  std::istringstream is(s);
  std::string line;
  while (std::getline(is, line)) {
    const std::string trimmed = ltrim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }
    std::istringstream ls(trimmed);
    std::string keyword;
    ls >> keyword;
    return keyword;
  }
  return {};
}

std::string jsonEscape(const std::string &s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c;
    }
  }
  return out;
}

void printJsonStrings(const std::vector<std::string> &items) {
  std::cout << "[";
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      std::cout << ",";
    }
    std::cout << "\"" << jsonEscape(items[i]) << "\"";
  }
  std::cout << "]";
}

std::vector<std::string> splitCsv(const std::string &s) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start <= s.size()) {
    const std::size_t comma = s.find(',', start);
    const std::size_t end = (comma == std::string::npos) ? s.size() : comma;
    const std::string part = s.substr(start, end - start);
    if (!part.empty()) {
      out.push_back(part);
    }
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  return out;
}

bool parseFunctionSequence(const model::Model &model, const std::string &text,
                           gen::Sequence &sequence, std::string &error) {
  sequence.calls.clear();
  std::string rest = text;
  while (!rest.empty()) {
    const std::size_t semi = rest.find(';');
    std::string item = trim(rest.substr(0, semi));
    if (!item.empty()) {
      const std::size_t open = item.find('(');
      const std::size_t close = item.rfind(')');
      if (open == std::string::npos || close == std::string::npos || close < open) {
        error = "invalid call in --sequence: " + item;
        return false;
      }
      const std::string name = trim(item.substr(0, open));
      const std::string argsText = item.substr(open + 1, close - open - 1);

      const model::Function *function = nullptr;
      for (const auto &fn : model.functions) {
        if (fn.name == name) {
          function = &fn;
          break;
        }
      }
      if (function == nullptr) {
        error = "unknown function in --sequence: " + name;
        return false;
      }

      std::vector<std::string> argTokens;
      if (!trim(argsText).empty()) {
        std::size_t start = 0;
        while (start <= argsText.size()) {
          const std::size_t comma = argsText.find(',', start);
          const std::size_t end = (comma == std::string::npos) ? argsText.size() : comma;
          argTokens.push_back(trim(argsText.substr(start, end - start)));
          if (comma == std::string::npos) {
            break;
          }
          start = comma + 1;
        }
      }
      if (argTokens.size() != function->params.size()) {
        error = "argument count mismatch in --sequence for " + name;
        return false;
      }

      gen::Call call;
      call.function = name;
      for (std::size_t i = 0; i < function->params.size(); ++i) {
        const std::string &token = argTokens[i];
        const bool isResource =
            model.resources.find(function->params[i].type) != model.resources.end();
        if (isResource) {
          call.resourceArgs.push_back(token == "_" ? -1 : std::stoi(token));
          call.values.push_back("");
        } else {
          call.resourceArgs.push_back(-1);
          call.values.push_back(token == "_" ? "" : token);
        }
      }
      sequence.calls.push_back(call);
    }
    if (semi == std::string::npos) {
      break;
    }
    rest = rest.substr(semi + 1);
  }
  if (sequence.calls.empty()) {
    error = "empty --sequence";
    return false;
  }
  return true;
}

bool parseNonNegativeInt(const std::string &text, int &out) {
  if (text.empty()) {
    return false;
  }
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  std::size_t pos = 0;
  try {
    const long long value = std::stoll(text, &pos);
    if (pos != text.size() || value < 0 || value > 2147483647LL) {
      return false;
    }
    out = static_cast<int>(value);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

bool parseNonNegativeUnsigned(const std::string &text, unsigned &out) {
  if (text.empty()) {
    return false;
  }
  for (char c : text) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  std::size_t pos = 0;
  try {
    const unsigned long long value = std::stoull(text, &pos);
    if (pos != text.size() || value > 4294967295ULL) {
      return false;
    }
    out = static_cast<unsigned>(value);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

std::string transitionKey(const smodel::Transition &t) {
  return t.from + " -" + t.event + "-> " + t.to;
}

void collectNswitch(const smodel::StateMachine &machine, const std::string &state, int depth,
                    const std::string &prefix, std::set<std::string> &out) {
  if (depth == 0) {
    out.insert(prefix);
    return;
  }
  for (const auto &transition : machine.transitions) {
    if (transition.from != state) {
      continue;
    }
    const std::string key =
        prefix.empty() ? transitionKey(transition) : prefix + " ; " + transitionKey(transition);
    collectNswitch(machine, transition.to, depth - 1, key, out);
  }
}

int runFunction(const std::string &text, int maxLength, unsigned seed, bool json, bool negative,
                int maxCases, bool coverage, bool harness, bool dylib, bool randomAlgorithm,
                bool bfsAlgorithm, bool bindRandom, bool cover, int replayIndex, bool harnessJson,
                int tWay, int timeoutSeconds, const std::string &sequenceText) {
  antlr4::ANTLRInputStream input(text);
  FunctionDslLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  FunctionDslParser parser(&tokens);

  FunctionDslParser::ModelContext *tree = parser.model();
  if (parser.getNumberOfSyntaxErrors() != 0) {
    std::cerr << parser.getNumberOfSyntaxErrors() << " syntax error(s)" << std::endl;
    return 1;
  }

  FunctionModelBuilder builder;
  model::Model m = builder.build(tree);
  std::vector<gen::Sequence> sequences;
  std::vector<gen::Sequence> negativeSequences;
  if (!sequenceText.empty()) {
    gen::Sequence supplied;
    std::string error;
    if (!parseFunctionSequence(m, sequenceText, supplied, error)) {
      std::cerr << error << std::endl;
      return 1;
    }
    sequences.push_back(supplied);
  } else {
    gen::SequenceGenerator generator(m, maxLength, seed, negative, maxCases);
    generator.setBindRandom(bindRandom);
    if (bfsAlgorithm) {
      sequences = generator.generateBfs();
    } else if (randomAlgorithm) {
      sequences = generator.generateRandom(seed, maxCases);
    } else {
      sequences = generator.generate();
    }
    negativeSequences = generator.negativeSequences();
  }

  if (replayIndex >= 0) {
    if (replayIndex >= static_cast<int>(sequences.size())) {
      std::cerr << "replay index out of range: " << replayIndex << std::endl;
      return 1;
    }
    std::vector<gen::Sequence> one{sequences[replayIndex]};
    std::cout << harness::generate(m, one, false, harnessJson, timeoutSeconds);
    return 0;
  }

  if (harness) {
    std::cout << harness::generate(m, sequences, dylib, harnessJson, timeoutSeconds);
    return 0;
  }

  if (json) {
    std::vector<std::string> seqTexts;
    seqTexts.reserve(sequences.size());
    for (const auto &seq : sequences) {
      seqTexts.push_back(seq.text());
    }
    std::vector<std::string> negTexts;
    negTexts.reserve(negativeSequences.size());
    for (const auto &seq : negativeSequences) {
      negTexts.push_back(seq.text());
    }
    std::set<std::string> coveredFunctions;
    std::set<std::string> coveredPairs;
    std::set<std::string> coveredTway;
    if (coverage) {
      for (const auto &seq : sequences) {
        for (std::size_t i = 0; i < seq.calls.size(); ++i) {
          coveredFunctions.insert(seq.calls[i].function);
          if (i + 1 < seq.calls.size()) {
            coveredPairs.insert(seq.calls[i].function + " ; " + seq.calls[i + 1].function);
          }
          if (tWay >= 1 && i + tWay <= seq.calls.size()) {
            std::string key;
            for (int k = 0; k < tWay; ++k) {
              if (k > 0) key += " ; ";
              key += seq.calls[i + k].function;
            }
            coveredTway.insert(key);
          }
        }
      }
    }
    std::cout << "{\"kind\":\"function\"";
    std::cout << ",\"version\":\"" << kToolVersion << "\"";
    std::cout << ",\"model_hash\":\"" << modelHash(text) << "\"";
    std::cout << ",\"seed\":" << seed;
    std::cout << ",\"types\":" << m.typeMap.size();
    std::cout << ",\"values\":" << m.values.size();
    std::cout << ",\"resources\":" << m.resources.size();
    std::cout << ",\"functions\":" << m.functions.size();
    std::cout << ",\"sequences\":";
    printJsonStrings(seqTexts);
    std::cout << ",\"negative_sequences\":";
    printJsonStrings(negTexts);
    if (coverage) {
      std::cout << ",\"coverage\":{";
      std::cout << "\"functions\":" << coveredFunctions.size() << "/" << m.functions.size();
      std::cout << ",\"function_pairs\":" << coveredPairs.size() << "/"
                << (m.functions.size() * m.functions.size());
      if (tWay >= 1) {
        std::size_t total = 1;
        for (int k = 0; k < tWay; ++k) {
          total *= m.functions.size();
        }
        std::cout << ",\"tway_" << tWay << "\":" << coveredTway.size() << "/" << total;
      }
      std::cout << "}";
    }
    std::cout << ",\"errors\":";
    printJsonStrings(m.errors);
    std::cout << "}" << std::endl;
    return m.errors.empty() ? 0 : 1;
  }

  std::cout << "types: " << m.typeMap.size() << std::endl;
  std::cout << "values: " << m.values.size() << std::endl;
  std::cout << "resources: " << m.resources.size() << std::endl;
  std::cout << "functions: " << m.functions.size() << std::endl;

  if (!m.errors.empty()) {
    std::cerr << m.errors.size() << " semantic error(s)" << std::endl;
    for (const auto &e : m.errors) {
      std::cerr << "  " << e << std::endl;
    }
    return 1;
  }

  if (cover) {
    std::set<std::string> allFunctions;
    for (const auto &function : m.functions) {
      allFunctions.insert(function.name);
    }
    std::vector<std::set<std::string>> seqFunctions(sequences.size());
    for (std::size_t i = 0; i < sequences.size(); ++i) {
      for (const auto &call : sequences[i].calls) {
        seqFunctions[i].insert(call.function);
      }
    }
    std::set<std::string> uncovered = allFunctions;
    std::vector<std::size_t> selected;
    std::vector<bool> used(sequences.size(), false);
    while (!uncovered.empty()) {
      std::size_t best = sequences.size();
      std::size_t bestCount = 0;
      for (std::size_t i = 0; i < sequences.size(); ++i) {
        if (used[i]) {
          continue;
        }
        std::size_t count = 0;
        for (const auto &name : seqFunctions[i]) {
          if (uncovered.count(name)) {
            ++count;
          }
        }
        if (count > bestCount) {
          best = i;
          bestCount = count;
        }
      }
      if (best == sequences.size()) {
        break;
      }
      selected.push_back(best);
      used[best] = true;
      for (const auto &name : seqFunctions[best]) {
        uncovered.erase(name);
      }
    }
    std::cout << "covering sequences: " << selected.size() << "/" << sequences.size() << std::endl;
    for (const auto index : selected) {
      std::cout << "  " << sequences[index].text() << std::endl;
    }
    std::cout << "covered functions: " << (allFunctions.size() - uncovered.size()) << "/"
              << allFunctions.size() << std::endl;
    std::cout << "OK" << std::endl;
    return 0;
  }

  std::cout << "sequences: " << sequences.size() << std::endl;
  for (const auto &seq : sequences) {
    std::cout << "  " << seq.text() << std::endl;
  }
  if (negative) {
    std::cout << "negative: " << negativeSequences.size() << std::endl;
    for (const auto &seq : negativeSequences) {
      std::cout << "  " << seq.text() << std::endl;
    }
  }
  if (coverage) {
    std::set<std::string> covered;
    std::set<std::string> coveredPairs;
    std::set<std::string> coveredTway;
    for (const auto &seq : sequences) {
      for (std::size_t i = 0; i < seq.calls.size(); ++i) {
        const auto &call = seq.calls[i];
        covered.insert(call.function);
        if (i + 1 < seq.calls.size()) {
          coveredPairs.insert(call.function + " ; " + seq.calls[i + 1].function);
        }
        if (tWay >= 1 && i + tWay <= seq.calls.size()) {
          std::string key;
          for (int k = 0; k < tWay; ++k) {
            if (k > 0) key += " ; ";
            key += seq.calls[i + k].function;
          }
          coveredTway.insert(key);
        }
      }
    }
    std::cout << "covered_functions: " << covered.size() << "/" << m.functions.size() << std::endl;
    std::cout << "covered_function_pairs: " << coveredPairs.size() << "/"
              << (m.functions.size() * m.functions.size()) << std::endl;
    if (tWay >= 1) {
      std::size_t total = 1;
      for (int k = 0; k < tWay; ++k) {
        total *= m.functions.size();
      }
      std::cout << "covered_tway_" << tWay << ": " << coveredTway.size() << "/" << total
                << std::endl;
    }
  }
  std::cout << "OK" << std::endl;
  return 0;
}

int runStateMachine(const std::string &text, int maxLength, bool json, bool coverage, bool cover,
                    bool randomAlgorithm, bool tourAlgorithm, bool bfsAlgorithm, unsigned seed, int maxCases,
                    const std::string &events, int replayIndex, int nSwitch,
                    const std::map<std::string, std::string> &guardValues, bool negative,
                    bool harness, int timeoutSeconds) {
  antlr4::ANTLRInputStream input(text);
  StateMachineDslLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  StateMachineDslParser parser(&tokens);

  StateMachineDslParser::MachineContext *tree = parser.machine();
  if (parser.getNumberOfSyntaxErrors() != 0) {
    std::cerr << parser.getNumberOfSyntaxErrors() << " syntax error(s)" << std::endl;
    return 1;
  }

  StateMachineBuilder builder;
  smodel::StateMachine m = builder.build(tree);
  if (harness) {
    if (!m.errors.empty()) {
      std::cerr << m.errors.size() << " semantic error(s)" << std::endl;
      for (const auto &e : m.errors) {
        std::cerr << "  " << e << std::endl;
      }
      return 1;
    }
    if (events.empty()) {
      std::cerr << "state machine harness requires --events" << std::endl;
      return 2;
    }
    std::cout << harness::generateStateMachine(m, splitCsv(events), guardValues, timeoutSeconds);
    return 0;
  }
  spath::StateMachinePathGenerator generator(m, maxLength, guardValues, maxCases);
  std::vector<spath::Path> paths;
  if (tourAlgorithm) {
    paths = generator.generateTour();
  } else if (bfsAlgorithm) {
    paths = generator.generateBfs();
  } else if (randomAlgorithm) {
    paths = generator.generateRandom(seed, maxCases);
  } else {
    paths = generator.generate();
  }

  std::vector<std::string> negatives;
  if (negative) {
    for (const auto &state : m.states) {
      const smodel::StateInfo *info = smodel::findState(m, state);
      if (info != nullptr && !info->children.empty()) {
        continue;
      }
      for (const auto &event : m.events) {
        const bool accepted = std::any_of(
            m.transitions.begin(), m.transitions.end(), [&](const smodel::Transition &t) {
              return t.event == event && (t.from == state || smodel::isDescendantOf(m, state, t.from));
            });
        if (!accepted) {
          negatives.push_back(state + " does not accept " + event);
        }
      }
    }
  }

  if (!events.empty()) {
    std::vector<std::string> trace;
    std::vector<std::string> actions;
    bool failed = false;
    std::string failure;

    std::map<std::string, std::string> historyOwner;
    std::map<std::string, std::string> historyDeepOwner;
    for (const auto &[name, info] : m.stateInfo) {
      (void)name;
      if (!info.history.empty()) {
        historyOwner[info.history] = name;
      }
      if (!info.historyDeep.empty()) {
        historyDeepOwner[info.historyDeep] = name;
      }
    }
    std::map<std::string, std::string> lastChild;
    std::map<std::string, std::string> lastLeaf;

    auto enterSet = [&](const std::string &state) -> std::set<std::string> {
      const smodel::StateInfo *info = smodel::findState(m, state);
      if (info != nullptr && info->concurrent) {
        std::set<std::string> out;
        for (const auto &child : info->children) {
          out.insert(smodel::leafOf(m, child));
        }
        if (out.empty()) {
          out.insert(smodel::leafOf(m, state));
        }
        return out;
      }
      return {smodel::leafOf(m, state)};
    };

    auto resolveTarget = [&](const std::string &to) -> std::set<std::string> {
      auto deep = historyDeepOwner.find(to);
      if (deep != historyDeepOwner.end()) {
        auto last = lastLeaf.find(deep->second);
        return {last != lastLeaf.end() ? last->second : smodel::leafOf(m, deep->second)};
      }
      auto shallow = historyOwner.find(to);
      if (shallow != historyOwner.end()) {
        auto child = lastChild.find(shallow->second);
        return {smodel::leafOf(m,
                               child != lastChild.end() ? child->second
                                                        : smodel::findState(m, shallow->second)->initial)};
      }
      return enterSet(to);
    };

    auto updateHistory = [&](const std::string &leaf) {
      std::string current = leaf;
      std::string child = leaf;
      while (!current.empty()) {
        const smodel::StateInfo *info = smodel::findState(m, current);
        if (info == nullptr || info->parent.empty()) {
          break;
        }
        lastChild[info->parent] = child;
        lastLeaf[info->parent] = leaf;
        child = info->parent;
        current = info->parent;
      }
    };

    std::set<std::string> active = enterSet(m.initial);
    for (const auto &leaf : active) {
      updateHistory(leaf);
    }

    auto activeText = [&]() {
      std::string text;
      for (const auto &leaf : active) {
        if (!text.empty()) {
          text += ",";
        }
        text += leaf;
      }
      return text;
    };

    for (const auto &event : splitCsv(events)) {
      const smodel::Transition *chosen = nullptr;
      std::string guardError;
      for (const auto &leaf : active) {
        for (const auto &transition : m.transitions) {
          if (!(transition.from == leaf ||
                smodel::isDescendantOf(m, leaf, transition.from))) {
            continue;
          }
          if (transition.event != event) {
            continue;
          }
          if (transition.guard.empty()) {
            chosen = &transition;
            break;
          }
          std::string error;
          if (guard::evalGuard(transition.guard, guardValues, &error)) {
            chosen = &transition;
            break;
          }
          if (!error.empty()) {
            guardError = error;
          }
        }
        if (chosen != nullptr || !guardError.empty()) {
          break;
        }
      }
      if (chosen == nullptr) {
        failed = true;
        failure = guardError.empty()
                      ? "no transition for event " + event + " in active states " + activeText()
                      : "guard " + guardError + " for event " + event;
        break;
      }

      trace.push_back(chosen->from + " -" + event + "-> " + chosen->to);
      const smodel::StateInfo *fromInfo = smodel::findState(m, chosen->from);
      if (fromInfo != nullptr && !fromInfo->exit.empty()) {
        actions.push_back("exit " + chosen->from + ": " + fromInfo->exit);
      }
      if (!chosen->action.empty()) {
        actions.push_back("action: " + chosen->action);
      }
      for (auto it = active.begin(); it != active.end();) {
        if (*it == chosen->from || smodel::isDescendantOf(m, *it, chosen->from)) {
          it = active.erase(it);
        } else {
          ++it;
        }
      }
      const std::set<std::string> targets = resolveTarget(chosen->to);
      for (const auto &target : targets) {
        active.insert(target);
        updateHistory(target);
        const smodel::StateInfo *targetInfo = smodel::findState(m, target);
        if (targetInfo != nullptr && !targetInfo->entry.empty()) {
          actions.push_back("entry " + target + ": " + targetInfo->entry);
        }
      }
    }

    const std::string finalState = activeText();

    if (json) {
      std::cout << "{\"kind\":\"state_machine_execution\"";
      std::cout << ",\"machine\":\"" << jsonEscape(m.name) << "\"";
      std::cout << ",\"trace\":";
      printJsonStrings(trace);
      std::cout << ",\"actions\":";
      printJsonStrings(actions);
      std::cout << ",\"final\":\"" << jsonEscape(finalState) << "\"";
      std::cout << ",\"failed\":" << (failed ? "true" : "false");
      std::cout << ",\"failure\":\"" << jsonEscape(failure) << "\"";
      std::cout << "}" << std::endl;
      return failed ? 1 : 0;
    }

    for (const auto &step : trace) {
      std::cout << step << std::endl;
    }
    for (const auto &action : actions) {
      std::cout << "  " << action << std::endl;
    }
    if (failed) {
      std::cout << "FAIL: " << failure << std::endl;
      return 1;
    }
    std::cout << "final: " << finalState << std::endl;
    std::cout << "OK" << std::endl;
    return 0;
  }

  if (json) {
    std::vector<std::string> pathTexts;
    pathTexts.reserve(paths.size());
    for (const auto &path : paths) {
      pathTexts.push_back(path.text());
    }
    std::set<std::string> coveredStates;
    std::set<std::string> coveredTransitions;
    std::set<std::string> coveredNswitch;
    std::set<std::string> totalNswitch;
    if (coverage) {
      for (const auto &path : paths) {
        for (const auto &step : path.steps) {
          coveredStates.insert(step.transition.from);
          coveredStates.insert(step.transition.to);
          for (const auto &leaf : smodel::enterLeaves(m, step.transition.to)) {
            coveredStates.insert(leaf);
          }
          coveredTransitions.insert(transitionKey(step.transition));
        }
        if (nSwitch >= 1) {
          for (std::size_t i = 0; i + nSwitch <= path.steps.size(); ++i) {
            std::string key;
            for (int k = 0; k < nSwitch; ++k) {
              if (k > 0) key += " ; ";
              key += transitionKey(path.steps[i + k].transition);
            }
            coveredNswitch.insert(key);
          }
        }
      }
      if (nSwitch >= 1) {
        for (const auto &state : m.states) {
          collectNswitch(m, state, nSwitch, "", totalNswitch);
        }
      }
    }
    std::cout << "{\"kind\":\"state_machine\"";
    std::cout << ",\"version\":\"" << kToolVersion << "\"";
    std::cout << ",\"model_hash\":\"" << modelHash(text) << "\"";
    std::cout << ",\"seed\":" << seed;
    std::cout << ",\"machine\":\"" << jsonEscape(m.name) << "\"";
    std::cout << ",\"states\":" << m.states.size();
    std::cout << ",\"events\":" << m.events.size();
    std::cout << ",\"transitions\":" << m.transitions.size();
    std::cout << ",\"paths\":";
    printJsonStrings(pathTexts);
    if (coverage) {
      std::cout << ",\"coverage\":{";
      std::cout << "\"states\":" << coveredStates.size() << "/" << m.states.size();
      std::cout << ",\"transitions\":" << coveredTransitions.size() << "/"
                << m.transitions.size();
      if (nSwitch >= 1) {
        std::cout << ",\"nswitch_" << nSwitch << "\":" << coveredNswitch.size() << "/"
                  << totalNswitch.size();
      }
      std::cout << "}";
    }
    if (negative) {
      std::cout << ",\"negative\":";
      printJsonStrings(negatives);
    }
    if (!generator.skippedGuards().empty()) {
      const std::vector<std::string> skipped(generator.skippedGuards().begin(),
                                             generator.skippedGuards().end());
      std::cout << ",\"skipped_guards\":";
      printJsonStrings(skipped);
    }
    if (tourAlgorithm && generator.truncated()) {
      const std::vector<std::string> uncovered(generator.uncoveredTransitions().begin(),
                                               generator.uncoveredTransitions().end());
      std::cout << ",\"truncated\":true";
      std::cout << ",\"uncovered_transitions\":";
      printJsonStrings(uncovered);
    }
    std::cout << ",\"errors\":";
    printJsonStrings(m.errors);
    std::cout << "}" << std::endl;
    return m.errors.empty() ? 0 : 1;
  }

  std::cout << "machine: " << m.name << std::endl;
  std::cout << "states: " << m.states.size() << std::endl;
  std::cout << "events: " << m.events.size() << std::endl;
  std::cout << "transitions: " << m.transitions.size() << std::endl;

  if (!m.errors.empty()) {
    std::cerr << m.errors.size() << " semantic error(s)" << std::endl;
    for (const auto &e : m.errors) {
      std::cerr << "  " << e << std::endl;
    }
    return 1;
  }

  if (negative) {
    std::cout << "negative: " << negatives.size() << std::endl;
    for (const auto &item : negatives) {
      std::cout << "  " << item << std::endl;
    }
  }

  if (replayIndex >= 0) {
    if (replayIndex >= static_cast<int>(paths.size())) {
      std::cerr << "replay index out of range: " << replayIndex << std::endl;
      return 1;
    }
    std::cout << paths[replayIndex].text() << std::endl;
    std::string csv;
    for (std::size_t i = 0; i < paths[replayIndex].steps.size(); ++i) {
      if (i > 0) {
        csv += ",";
      }
      csv += paths[replayIndex].steps[i].transition.event;
    }
    std::cout << "replay-events: " << csv << std::endl;
    return 0;
  }

  if (cover) {
    std::set<std::string> allTransitions;
    for (const auto &transition : m.transitions) {
      allTransitions.insert(transitionKey(transition));
    }
    std::vector<std::set<std::string>> pathTransitions(paths.size());
    for (std::size_t i = 0; i < paths.size(); ++i) {
      for (const auto &step : paths[i].steps) {
        pathTransitions[i].insert(transitionKey(step.transition));
      }
    }

    std::set<std::string> uncovered = allTransitions;
    std::vector<std::size_t> selected;
    std::vector<bool> used(paths.size(), false);
    while (!uncovered.empty()) {
      std::size_t best = paths.size();
      std::size_t bestCount = 0;
      for (std::size_t i = 0; i < paths.size(); ++i) {
        if (used[i]) {
          continue;
        }
        std::size_t count = 0;
        for (const auto &key : pathTransitions[i]) {
          if (uncovered.count(key)) {
            ++count;
          }
        }
        if (count > bestCount) {
          best = i;
          bestCount = count;
        }
      }
      if (best == paths.size()) {
        break;
      }
      selected.push_back(best);
      used[best] = true;
      for (const auto &key : pathTransitions[best]) {
        uncovered.erase(key);
      }
    }

    std::cout << "covering paths: " << selected.size() << "/" << paths.size() << std::endl;
    for (const auto index : selected) {
      std::cout << "  " << paths[index].text() << std::endl;
    }
    std::cout << "covered transitions: " << (allTransitions.size() - uncovered.size()) << "/"
              << allTransitions.size() << std::endl;
    std::cout << "OK" << std::endl;
    return 0;
  }

  std::cout << "paths: " << paths.size() << std::endl;
  for (const auto &path : paths) {
    std::cout << "  " << path.text() << std::endl;
  }
  if (!generator.skippedGuards().empty()) {
    std::cout << "skipped_guards: " << generator.skippedGuards().size() << std::endl;
    for (const auto &reason : generator.skippedGuards()) {
      std::cout << "  " << reason << std::endl;
    }
  }
  if (tourAlgorithm && generator.truncated()) {
    std::cout << "tour truncated: true" << std::endl;
    std::cout << "uncovered_transitions: " << generator.uncoveredTransitions().size()
              << std::endl;
    for (const auto &transition : generator.uncoveredTransitions()) {
      std::cout << "  " << transition << std::endl;
    }
  }
  if (coverage) {
    std::set<std::string> coveredStates;
    std::set<std::string> coveredTransitions;
    for (const auto &path : paths) {
      for (const auto &step : path.steps) {
        coveredStates.insert(step.transition.from);
        coveredStates.insert(step.transition.to);
        for (const auto &leaf : smodel::enterLeaves(m, step.transition.to)) {
          coveredStates.insert(leaf);
        }
        coveredTransitions.insert(step.transition.from + " -" + step.transition.event + "-> " +
                                  step.transition.to);
      }
    }
    std::cout << "covered_states: " << coveredStates.size() << "/" << m.states.size() << std::endl;
    std::cout << "covered_transitions: " << coveredTransitions.size() << "/" << m.transitions.size()
              << std::endl;

    if (nSwitch >= 1) {
      std::set<std::string> totalNswitch;
      for (const auto &state : m.states) {
        collectNswitch(m, state, nSwitch, "", totalNswitch);
      }
      std::set<std::string> coveredNswitch;
      for (const auto &path : paths) {
        for (std::size_t i = 0; i + nSwitch <= path.steps.size(); ++i) {
          std::string key;
          for (int k = 0; k < nSwitch; ++k) {
            if (k > 0) {
              key += " ; ";
            }
            key += transitionKey(path.steps[i + k].transition);
          }
          coveredNswitch.insert(key);
        }
      }
      std::cout << "covered_nswitch_" << nSwitch << ": " << coveredNswitch.size() << "/"
                << totalNswitch.size() << std::endl;
    }
  }
  std::cout << "OK" << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0]
              << " <model.dsl> [--max-length N] [--seed N] [--json] [--negative] [--coverage]"
              << " [--cover] [--algorithm dfs|bfs|random|tour] [--harness] [--harness-json] [--dylib] [--events e1,e2,...]"
              << " [--bind enumerate|random] [--n-switch N] [--t-way N] [--guard k=v] [--replay N] [--max-cases N]"
              << " [--timeout N] [--sequence \"f(...);...\"]"
              << std::endl;
    return 2;
  }

  int maxLength = 3;
  unsigned seed = 0;
  bool json = false;
  bool negative = false;
  bool coverage = false;
  bool cover = false;
  bool harness = false;
  bool dylib = false;
  bool harnessJson = false;
  bool randomAlgorithm = false;
  bool tourAlgorithm = false;
  bool bfsAlgorithm = false;
  bool bindRandom = false;
  int nSwitch = 0;
  int tWay = 0;
  int maxCases = 0;
  int timeoutSeconds = 10;
  int replayIndex = -1;
  std::string events;
  std::string sequenceText;
  std::map<std::string, std::string> guardValues;
  std::string modelPath;

  try {
    for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--json") {
      json = true;
    } else if (arg == "--negative") {
      negative = true;
    } else if (arg == "--coverage") {
      coverage = true;
    } else if (arg == "--cover") {
      cover = true;
    } else if (arg == "--algorithm" && i + 1 < argc) {
      const std::string algorithm = argv[++i];
      if (algorithm == "random") {
        randomAlgorithm = true;
      } else if (algorithm == "tour") {
        tourAlgorithm = true;
      } else if (algorithm == "bfs") {
        bfsAlgorithm = true;
      } else if (algorithm == "dfs") {
        // dfs is the default; accepting the explicit option keeps the CLI
        // contract consistent with README.
      } else {
        std::cerr << "unknown algorithm: " << algorithm << std::endl;
        return 2;
      }
    } else if (arg == "--bind" && i + 1 < argc) {
      const std::string bindMode = argv[++i];
      if (bindMode == "random") {
        bindRandom = true;
      } else if (bindMode == "enumerate") {
        bindRandom = false;
      } else {
        std::cerr << "unknown bind mode: " << bindMode << std::endl;
        return 2;
      }
    } else if (arg == "--n-switch" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!parseNonNegativeInt(value, nSwitch)) {
        std::cerr << "invalid value for --n-switch: " << value << std::endl;
        return 2;
      }
      coverage = true;
    } else if (arg == "--t-way" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!parseNonNegativeInt(value, tWay) || tWay <= 0) {
        std::cerr << "invalid value for --t-way: " << value << std::endl;
        return 2;
      }
      coverage = true;
    } else if (arg == "--harness") {
      harness = true;
    } else if (arg == "--dylib") {
      harness = true;
      dylib = true;
    } else if (arg == "--harness-json") {
      harness = true;
      harnessJson = true;
    } else if (arg == "--events" && i + 1 < argc) {
      events = argv[++i];
    } else if (arg == "--sequence" && i + 1 < argc) {
      sequenceText = argv[++i];
    } else if (arg == "--guard" && i + 1 < argc) {
      const std::string spec = argv[++i];
      const std::size_t eq = spec.find('=');
      if (eq == std::string::npos) {
        std::cerr << "invalid --guard value: " << spec << std::endl;
        return 2;
      }
      guardValues[spec.substr(0, eq)] = spec.substr(eq + 1);
    } else if (arg == "--max-length" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!parseNonNegativeInt(value, maxLength)) {
        std::cerr << "invalid value for --max-length: " << value << std::endl;
        return 2;
      }
    } else if (arg == "--seed" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!parseNonNegativeUnsigned(value, seed)) {
        std::cerr << "invalid value for --seed: " << value << std::endl;
        return 2;
      }
    } else if (arg == "--max-cases" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!parseNonNegativeInt(value, maxCases)) {
        std::cerr << "invalid value for --max-cases: " << value << std::endl;
        return 2;
      }
    } else if (arg == "--timeout" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!parseNonNegativeInt(value, timeoutSeconds)) {
        std::cerr << "invalid value for --timeout: " << value << std::endl;
        return 2;
      }
    } else if (arg == "--replay" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (!parseNonNegativeInt(value, replayIndex)) {
        std::cerr << "invalid value for --replay: " << value << std::endl;
        return 2;
      }
    } else if (modelPath.empty()) {
      modelPath = arg;
    } else {
      std::cerr << "unknown argument: " << arg << std::endl;
      return 2;
    }
    }
  } catch (const std::exception &) {
    std::cerr << "invalid numeric argument" << std::endl;
    return 2;
  }

  if (modelPath.empty()) {
    std::cerr << "missing model file" << std::endl;
    return 2;
  }

  std::ifstream in(modelPath);
  if (!in) {
    std::cerr << "cannot open " << modelPath << std::endl;
    return 2;
  }

  std::stringstream ss;
  ss << in.rdbuf();
  const std::string text = ss.str();

  try {
    const bool isMachine = (firstKeyword(text) == "machine");
    if (isMachine && (dylib || bindRandom)) {
      std::cerr << "warning: --dylib/--bind are ignored for state machine models"
                << std::endl;
    }
    if (isMachine) {
      return runStateMachine(text, maxLength, json, coverage, cover, randomAlgorithm, tourAlgorithm,
                             bfsAlgorithm, seed, maxCases, events, replayIndex, nSwitch, guardValues,
                             negative, harness, timeoutSeconds);
    }
    return runFunction(text, maxLength, seed, json, negative, maxCases, coverage, harness, dylib,
                       randomAlgorithm, bfsAlgorithm, bindRandom, cover, replayIndex, harnessJson,
                       tWay, timeoutSeconds, sequenceText);
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 3;
  }
}
