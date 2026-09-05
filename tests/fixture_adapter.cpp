#include <iostream>
#include <string>
int main() { std::string line; if (!std::getline(std::cin, line)) return 2; std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\"}\n"; return 0; }
