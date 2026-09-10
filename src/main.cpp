#include <fstream>
#include <iostream>
#include <sstream>

#include "FunctionDslLexer.h"
#include "FunctionDslParser.h"
#include "antlr4-runtime.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <model.dsl>" << std::endl;
    return 2;
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

    parser.model();
    std::size_t errors = parser.getNumberOfSyntaxErrors();
    if (errors == 0) {
      std::cout << "OK" << std::endl;
      return 0;
    }

    std::cerr << errors << " syntax error(s)" << std::endl;
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << std::endl;
    return 3;
  }
}
