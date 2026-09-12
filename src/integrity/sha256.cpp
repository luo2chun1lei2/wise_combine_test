#include "integrity/sha256.hpp"
#include "spec/spec.hpp"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace wise::integrity {
namespace { std::string escape_json(const std::string& value) { std::ostringstream out; for (const char c : value) { if (c == '\\' || c == '"') out << '\\' << c; else if (c == '\n') out << "\\n"; else if (c == '\r') out << "\\r"; else if (c == '\t') out << "\\t"; else out << c; } return out.str(); } }
std::string sha256_hex(const std::string& input) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto byte : digest) out << std::setw(2) << static_cast<unsigned>(byte);
  return out.str();
}
std::string wrap_v2(const std::string& payload) {
  return "{\"schema_version\":2,\"payload\":\"" + escape_json(payload) + "\",\"integrity\":{\"algorithm\":\"sha256\",\"digest\":\"" + sha256_hex(payload) + "\"}}";
}
bool verify_v2(const std::string& document, std::string* payload) {
  try { const auto envelope = spec::parse_integrity_envelope(document); if (sha256_hex(envelope.payload) != envelope.digest) return false; if (payload != nullptr) *payload = envelope.payload; return true; } catch (const std::exception&) { return false; }
}
}
