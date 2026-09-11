#include <algorithm>
#include <cctype>
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
#include "harness_generator.h"
#include "sequence_generator.h"
#include "state_machine_builder.h"
#include "state_machine_path_generator.h"

namespace {

std::string ltrim(const std::string &s) {
  const auto pos = s.find_first_not_of(" \t\r\n");
  return pos == std::string::npos ? "" : s.substr(pos);
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

bool isNumber(const std::string &s) {
  if (s.empty()) {
    return false;
  }
  std::size_t i = (s[0] == '-') ? 1 : 0;
  if (i == s.size()) {
    return false;
  }
  for (; i < s.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
      return false;
    }
  }
  return true;
}

bool evalGuard(const std::string &guard, const std::map<std::string, std::string> &values) {
  if (guard.empty()) {
    return true;
  }

  struct Token {
    std::string text;
    std::string kind;  // "id" | "num" | "str" | "op"
  };
  std::vector<Token> tokens;
  for (std::size_t i = 0; i < guard.size();) {
    const char c = guard[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
    } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::size_t j = i;
      while (j < guard.size() &&
             (std::isalnum(static_cast<unsigned char>(guard[j])) || guard[j] == '_')) {
        ++j;
      }
      tokens.push_back({guard.substr(i, j - i), "id"});
      i = j;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      std::size_t j = i;
      while (j < guard.size() && std::isdigit(static_cast<unsigned char>(guard[j]))) {
        ++j;
      }
      tokens.push_back({guard.substr(i, j - i), "num"});
      i = j;
    } else if (c == '"') {
      std::size_t j = i + 1;
      while (j < guard.size() && guard[j] != '"') {
        ++j;
      }
      if (j < guard.size()) {
        ++j;
      }
      tokens.push_back({guard.substr(i, j - i), "str"});
      i = j;
    } else if (i + 1 < guard.size() &&
               (guard.substr(i, 2) == "==" || guard.substr(i, 2) == "!=" ||
                guard.substr(i, 2) == ">=" || guard.substr(i, 2) == "<=" ||
                guard.substr(i, 2) == "&&" || guard.substr(i, 2) == "||")) {
      tokens.push_back({guard.substr(i, 2), "op"});
      i += 2;
    } else if (c == '>' || c == '<' || c == '(' || c == ')') {
      tokens.push_back({std::string(1, c), "op"});
      ++i;
    } else {
      ++i;
    }
  }

  std::size_t index = 0;
  auto peek = [&]() -> const Token * {
    return index < tokens.size() ? &tokens[index] : nullptr;
  };
  auto consume = [&](const std::string &text) -> bool {
    if (peek() != nullptr && peek()->text == text) {
      ++index;
      return true;
    }
    return false;
  };

  std::function<bool()> parseOr;
  std::function<bool()> parseAnd;
  std::function<bool()> parsePrimary;

  parsePrimary = [&]() -> bool {
    if (consume("(")) {
      const bool value = parseOr();
      consume(")");
      return value;
    }
    const Token *left = peek();
    if (left == nullptr || left->kind != "id") {
      return true;
    }
    ++index;
    const Token *op = peek();
    if (op == nullptr || op->kind != "op") {
      return true;
    }
    ++index;
    const Token *right = peek();
    std::string rightValue;
    if (right == nullptr) {
      return true;
    }
    if (right->kind == "num") {
      rightValue = right->text;
    } else if (right->kind == "str") {
      rightValue = right->text.substr(1, right->text.size() - 2);
    } else if (right->kind == "id") {
      auto it = values.find(right->text);
      rightValue = it != values.end() ? it->second : right->text;
    } else {
      return true;
    }
    ++index;

    auto it = values.find(left->text);
    if (it == values.end()) {
      return true;
    }
    const std::string leftValue = it->second;

    if (op->text == "==") return leftValue == rightValue;
    if (op->text == "!=") return leftValue != rightValue;
    if (isNumber(leftValue) && isNumber(rightValue)) {
      const long long a = std::stoll(leftValue);
      const long long b = std::stoll(rightValue);
      if (op->text == ">") return a > b;
      if (op->text == ">=") return a >= b;
      if (op->text == "<") return a < b;
      return a <= b;
    }
    if (op->text == ">") return leftValue > rightValue;
    if (op->text == ">=") return leftValue >= rightValue;
    if (op->text == "<") return leftValue < rightValue;
    return leftValue <= rightValue;
  };

  parseAnd = [&]() -> bool {
    bool value = parsePrimary();
    while (consume("&&")) {
      value = parsePrimary() && value;
    }
    return value;
  };

  parseOr = [&]() -> bool {
    bool value = parseAnd();
    while (consume("||")) {
      value = parseAnd() || value;
    }
    return value;
  };

  return parseOr();
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
                bool bfsAlgorithm, bool bindRandom, bool cover, int replayIndex, bool harnessJson) {
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
  gen::SequenceGenerator generator(m, maxLength, seed, negative, maxCases);
  generator.setBindRandom(bindRandom);
  std::vector<gen::Sequence> sequences;
  if (bfsAlgorithm) {
    sequences = generator.generateBfs();
  } else if (randomAlgorithm) {
    sequences = generator.generateRandom(seed, maxCases);
  } else {
    sequences = generator.generate();
  }
  const std::vector<gen::Sequence> &negativeSequences = generator.negativeSequences();

  if (replayIndex >= 0) {
    if (replayIndex >= static_cast<int>(sequences.size())) {
      std::cerr << "replay index out of range: " << replayIndex << std::endl;
      return 1;
    }
    std::vector<gen::Sequence> one{sequences[replayIndex]};
    std::cout << harness::generate(m, one, false, harnessJson);
    return 0;
  }

  if (harness) {
    std::cout << harness::generate(m, sequences, dylib, harnessJson);
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
    if (coverage) {
      for (const auto &seq : sequences) {
        for (std::size_t i = 0; i < seq.calls.size(); ++i) {
          coveredFunctions.insert(seq.calls[i].function);
          if (i + 1 < seq.calls.size()) {
            coveredPairs.insert(seq.calls[i].function + " ; " + seq.calls[i + 1].function);
          }
        }
      }
    }
    std::cout << "{\"kind\":\"function\"";
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
    for (const auto &seq : sequences) {
      for (std::size_t i = 0; i < seq.calls.size(); ++i) {
        const auto &call = seq.calls[i];
        covered.insert(call.function);
        if (i + 1 < seq.calls.size()) {
          coveredPairs.insert(call.function + " ; " + seq.calls[i + 1].function);
        }
      }
    }
    std::cout << "covered_functions: " << covered.size() << "/" << m.functions.size() << std::endl;
    std::cout << "covered_function_pairs: " << coveredPairs.size() << "/"
              << (m.functions.size() * m.functions.size()) << std::endl;
  }
  std::cout << "OK" << std::endl;
  return 0;
}

int runStateMachine(const std::string &text, int maxLength, bool json, bool coverage, bool cover,
                    bool randomAlgorithm, bool tourAlgorithm, bool bfsAlgorithm, unsigned seed, int maxCases,
                    const std::string &events, int replayIndex, int nSwitch,
                    const std::map<std::string, std::string> &guardValues, bool negative) {
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
  spath::StateMachinePathGenerator generator(m, maxLength);
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
      for (const auto &leaf : active) {
        auto it = std::find_if(m.transitions.begin(), m.transitions.end(),
                               [&](const smodel::Transition &t) {
                                 return (t.from == leaf || smodel::isDescendantOf(m, leaf, t.from)) &&
                                        t.event == event && evalGuard(t.guard, guardValues);
                               });
        if (it != m.transitions.end()) {
          chosen = &*it;
          break;
        }
      }
      if (chosen == nullptr) {
        failed = true;
        failure = "no transition for event " + event + " in active states " + activeText();
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
  if (coverage) {
    std::set<std::string> coveredStates;
    std::set<std::string> coveredTransitions;
    for (const auto &path : paths) {
      for (const auto &step : path.steps) {
        coveredStates.insert(step.transition.from);
        coveredStates.insert(step.transition.to);
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
              << " [--bind enumerate|random] [--n-switch N] [--guard k=v] [--replay N] [--max-cases N]"
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
  int maxCases = 0;
  int replayIndex = -1;
  std::string events;
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
      nSwitch = std::stoi(argv[++i]);
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
    } else if (arg == "--guard" && i + 1 < argc) {
      const std::string spec = argv[++i];
      const std::size_t eq = spec.find('=');
      if (eq == std::string::npos) {
        std::cerr << "invalid --guard value: " << spec << std::endl;
        return 2;
      }
      guardValues[spec.substr(0, eq)] = spec.substr(eq + 1);
    } else if (arg == "--max-length" && i + 1 < argc) {
      maxLength = std::stoi(argv[++i]);
    } else if (arg == "--seed" && i + 1 < argc) {
      seed = static_cast<unsigned>(std::stoul(argv[++i]));
    } else if (arg == "--max-cases" && i + 1 < argc) {
      maxCases = std::stoi(argv[++i]);
    } else if (arg == "--replay" && i + 1 < argc) {
      replayIndex = std::stoi(argv[++i]);
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
    if (isMachine && (harness || dylib || bindRandom)) {
      std::cerr << "warning: --harness/--dylib/--bind are ignored for state machine models"
                << std::endl;
    }
    if (isMachine) {
      return runStateMachine(text, maxLength, json, coverage, cover, randomAlgorithm, tourAlgorithm,
                             bfsAlgorithm, seed, maxCases, events, replayIndex, nSwitch, guardValues,
                             negative);
    }
    return runFunction(text, maxLength, seed, json, negative, maxCases, coverage, harness, dylib,
                       randomAlgorithm, bfsAlgorithm, bindRandom, cover, replayIndex, harnessJson);
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 3;
  }
}
