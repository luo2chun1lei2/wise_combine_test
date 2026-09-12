#include "integrity/sha256.hpp"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace wise::integrity {
std::string sha256_hex(const std::string& input) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto byte : digest) out << std::setw(2) << static_cast<unsigned>(byte);
  return out.str();
}
std::string wrap_v2(const std::string& payload) {
  return "{\"schema_version\":2,\"payload\":\"" + payload + "\",\"integrity\":{\"algorithm\":\"sha256\",\"digest\":\"" + sha256_hex(payload) + "\"}}";
}
bool verify_v2(const std::string& document, std::string* payload) {
  const std::string marker = "\"payload\":\""; const auto begin = document.find(marker); if (begin == std::string::npos) return false; const auto start = begin + marker.size(); const auto end = document.find("\",\"integrity\"", start); if (end == std::string::npos) return false; const auto value = document.substr(start, end - start); const std::string digest_marker = "\"digest\":\""; const auto digest = document.find(digest_marker, end); if (digest == std::string::npos) return false; const auto digest_start = digest + digest_marker.size(); const auto digest_end = document.find('"', digest_start); if (digest_end == std::string::npos || sha256_hex(value) != document.substr(digest_start, digest_end - digest_start)) return false; if (payload != nullptr) *payload = value; return true;
}
}
