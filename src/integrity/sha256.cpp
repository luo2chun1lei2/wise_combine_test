#include "integrity/sha256.hpp"
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
namespace wise::integrity {
std::string sha256_hex(const std::string& input) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (const auto byte : digest) out << std::setw(2) << static_cast<unsigned>(byte);
  return out.str();
}
}
