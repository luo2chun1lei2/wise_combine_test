#include <fstream>
#include <iostream>
#include <sstream>

#include "FunctionDslLexer.h"
#include "FunctionDslParser.h"
#include "antlr4-runtime.h"
#include "function_model_builder.h"
#include "sequence_generator.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <model.dsl> [max_length]" << std::endl;
    return 2;
  }

  int maxLength = 3;
  if (argc >= 3) {
    maxLength = std::stoi(argv[2]);
  }

  try {
    std::ifstream in(argv[1]);
    if (!in) {
      std::cerr << "cannot open " << argv[1] << std::endl;
      return 2;
    }

    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();

    antlr4::ANTLRInputStream input(text);
    FunctionDslLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    FunctionDslParser parser(&tokens);

    FunctionDslParser::ModelContext *tree = parser.model();
    std::size_t errors = parser.getNumberOfSyntaxErrors();
    if (errors != 0) {
      std::cerr << errors << " syntax error(s)" << std::endl;
      return 1;
    }

    FunctionModelBuilder builder;
    model::Model m = builder.build(tree);

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

    gen::SequenceGenerator generator(m, maxLength);
    std::vector<gen::Sequence> sequences = generator.generate();

    std::cout << "sequences: " << sequences.size() << std::endl;
    for (const auto &seq : sequences) {
      std::cout << "  " << seq.text() << std::endl;
    }
    std::cout << "OK" << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 3;
  }
}
