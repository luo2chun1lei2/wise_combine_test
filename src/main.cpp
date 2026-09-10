#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "FunctionDslLexer.h"
#include "FunctionDslParser.h"
#include "StateMachineDslLexer.h"
#include "StateMachineDslParser.h"
#include "antlr4-runtime.h"
#include "function_model_builder.h"
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

int runFunction(const std::string &text, int maxLength, unsigned seed, bool json) {
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
  gen::SequenceGenerator generator(m, maxLength, seed);
  std::vector<gen::Sequence> sequences = generator.generate();

  if (json) {
    std::vector<std::string> seqTexts;
    seqTexts.reserve(sequences.size());
    for (const auto &seq : sequences) {
      seqTexts.push_back(seq.text());
    }
    std::cout << "{\"kind\":\"function\"";
    std::cout << ",\"types\":" << m.typeMap.size();
    std::cout << ",\"values\":" << m.values.size();
    std::cout << ",\"resources\":" << m.resources.size();
    std::cout << ",\"functions\":" << m.functions.size();
    std::cout << ",\"sequences\":";
    printJsonStrings(seqTexts);
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
  std::cout << "OK" << std::endl;
  return 0;
}

int runStateMachine(const std::string &text, int maxLength, bool json) {
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

  std::cout << "paths: " << paths.size() << std::endl;
  for (const auto &path : paths) {
    std::cout << "  " << path.text() << std::endl;
  }
  std::cout << "OK" << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <model.dsl> [max_length] [seed] [json]" << std::endl;
    return 2;
  }

  int maxLength = 3;
  if (argc >= 3) {
    maxLength = std::stoi(argv[2]);
  }
  unsigned seed = 0;
  if (argc >= 4) {
    seed = static_cast<unsigned>(std::stoul(argv[3]));
  }
  bool json = (argc >= 5 && std::string(argv[4]) == "json");

  std::ifstream in(argv[1]);
  if (!in) {
    std::cerr << "cannot open " << argv[1] << std::endl;
    return 2;
  }

  std::stringstream ss;
  ss << in.rdbuf();
  const std::string text = ss.str();

  try {
    if (firstKeyword(text) == "machine") {
      return runStateMachine(text, maxLength, json);
    }
    return runFunction(text, maxLength, seed, json);
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 3;
  }
}
