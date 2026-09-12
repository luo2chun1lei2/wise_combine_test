#pragma once
#include <string>
namespace wise::integrity {
std::string sha256_hex(const std::string& input);
std::string wrap_v2(const std::string& payload);
bool verify_v2(const std::string& document, std::string* payload = nullptr);
}
