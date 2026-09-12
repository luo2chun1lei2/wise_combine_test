#include "integrity/sha256.hpp"
#include <iostream>
int main() { const std::string input = R"({"note":"quote \\"})"; const auto doc = wise::integrity::wrap_v2(input); std::string payload; if (!wise::integrity::verify_v2(doc, &payload) || payload != input) return 1; auto tampered = doc; tampered.replace(tampered.find("note"), 4, "FAIL"); if (wise::integrity::verify_v2(tampered)) return 1; std::cout << "integrity v2: all tests passed\n"; }
