#include "integrity/sha256.hpp"
#include <iostream>
int main() { const auto doc = wise::integrity::wrap_v2("abc"); std::string payload; if (!wise::integrity::verify_v2(doc, &payload) || payload != "abc") return 1; auto tampered = doc; tampered.replace(tampered.find("abc"), 3, "abd"); if (wise::integrity::verify_v2(tampered)) return 1; std::cout << "integrity v2: all tests passed\n"; }
