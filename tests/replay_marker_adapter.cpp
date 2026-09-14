#include <fstream>
#include <iostream>
#include <string>

#ifndef MARKER_TEXT
#define MARKER_TEXT "invoked"
#endif

int main(int argc, char** argv) {
  if (argc > 1) {
    std::ofstream marker(argv[1]);
    marker << MARKER_TEXT << '\n';
  }
  std::string line;
  if (!std::getline(std::cin, line)) return 2;
  if (line.find("\"function\":\"consume\"") != std::string::npos) {
    std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\","
                 "\"returns\":{},\"stderr\":\"\"}\n";
  } else {
    std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"ready\","
                 "\"returns\":{\"token\":\"from-producer\"},\"stderr\":\"\"}\n";
  }
}

// marker adapter relation fixture
