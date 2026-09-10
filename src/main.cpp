#include <algorithm>
#include <fstream>
#include <iostream>
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

int runFunction(const std::string &text, int maxLength, unsigned seed, bool json, bool negative,
                int maxCases, bool coverage, bool harness) {
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
  std::vector<gen::Sequence> sequences = generator.generate();
  const std::vector<gen::Sequence> &negativeSequences = generator.negativeSequences();

  if (harness) {
    std::cout << harness::generate(m, sequences);
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
    std::cout << "{\"kind\":\"function\"";
    std::cout << ",\"types\":" << m.typeMap.size();
    std::cout << ",\"values\":" << m.values.size();
    std::cout << ",\"resources\":" << m.resources.size();
    std::cout << ",\"functions\":" << m.functions.size();
    std::cout << ",\"sequences\":";
    printJsonStrings(seqTexts);
    std::cout << ",\"negative_sequences\":";
    printJsonStrings(negTexts);
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
    for (const auto &seq : sequences) {
      for (const auto &call : seq.calls) {
        covered.insert(call.function);
      }
    }
    std::cout << "covered_functions: " << covered.size() << "/" << m.functions.size() << std::endl;
  }
  std::cout << "OK" << std::endl;
  return 0;
}

int runStateMachine(const std::string &text, int maxLength, bool json, bool coverage,
                    const std::string &events) {
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
  std::vector<spath::Path> paths = generator.generate();

  if (json) {
    std::vector<std::string> pathTexts;
    pathTexts.reserve(paths.size());
    for (const auto &path : paths) {
      pathTexts.push_back(path.text());
    }
    std::cout << "{\"kind\":\"state_machine\"";
    std::cout << ",\"machine\":\"" << jsonEscape(m.name) << "\"";
    std::cout << ",\"states\":" << m.states.size();
    std::cout << ",\"events\":" << m.events.size();
    std::cout << ",\"transitions\":" << m.transitions.size();
    std::cout << ",\"paths\":";
    printJsonStrings(pathTexts);
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

  if (!events.empty()) {
    std::string current = m.initial;
    for (const auto &event : splitCsv(events)) {
      const auto it =
          std::find_if(m.transitions.begin(), m.transitions.end(), [&](const smodel::Transition &t) {
            return t.from == current && t.event == event;
          });
      if (it == m.transitions.end()) {
        std::cout << "FAIL: state " << current << " has no event " << event << std::endl;
        return 1;
      }
      std::cout << current << " -" << event << "-> " << it->to << std::endl;
      current = it->to;
    }
    std::cout << "final: " << current << std::endl;
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
  }
  std::cout << "OK" << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0]
              << " <model.dsl> [--max-length N] [--seed N] [--json] [--negative] [--coverage]"
              << " [--harness] [--events e1,e2,...] [--max-cases N]"
              << std::endl;
    return 2;
  }

  int maxLength = 3;
  unsigned seed = 0;
  bool json = false;
  bool negative = false;
  bool coverage = false;
  bool harness = false;
  int maxCases = 0;
  std::string events;
  std::string modelPath;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--json") {
      json = true;
    } else if (arg == "--negative") {
      negative = true;
    } else if (arg == "--coverage") {
      coverage = true;
    } else if (arg == "--harness") {
      harness = true;
    } else if (arg == "--events" && i + 1 < argc) {
      events = argv[++i];
    } else if (arg == "--max-length" && i + 1 < argc) {
      maxLength = std::stoi(argv[++i]);
    } else if (arg == "--seed" && i + 1 < argc) {
      seed = static_cast<unsigned>(std::stoul(argv[++i]));
    } else if (arg == "--max-cases" && i + 1 < argc) {
      maxCases = std::stoi(argv[++i]);
    } else if (modelPath.empty()) {
      modelPath = arg;
    } else {
      std::cerr << "unknown argument: " << arg << std::endl;
      return 2;
    }
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
    if (firstKeyword(text) == "machine") {
      return runStateMachine(text, maxLength, json, coverage, events);
    }
    return runFunction(text, maxLength, seed, json, negative, maxCases, coverage, harness);
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 3;
  }
}
