#include "integrity/sha256.hpp"
#include <iostream>
int main() { const std::string input = "quote \" slash \\ line\n"; const auto doc = wise::integrity::wrap_v2(input); std::string payload; if (!wise::integrity::verify_v2(doc, &payload) || payload != input) return 1; auto tampered = doc; tampered.replace(tampered.find("quote"), 5, "other"); if (wise::integrity::verify_v2(tampered)) return 1; std::cout << "integrity v2: all tests passed\n"; }
