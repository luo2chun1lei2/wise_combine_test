#include "integrity/sha256.hpp"

#include <iostream>

int main() {
  const std::string input = R"({"note":"quote \\"})";
  const auto document = wise::integrity::wrap_v2(input);
  std::string payload;
  if (!wise::integrity::verify_v2(document, &payload) || payload != input) return 1;

  auto tampered = document;
  tampered.replace(tampered.find("note"), 4, "FAIL");
  if (wise::integrity::verify_v2(tampered)) return 1;

  // The escaped payload is malformed JSON, while its envelope digest is correct.
  // The CLI collapses parser and digest failures, so source-order review covers
  // the parser-entry sequencing exemption; this case locks the public result.
  const std::string malformed = R"({"broken":})";
  const std::string malformed_document =
      R"({"schema_version":2,"payload":"{\"broken\":}","integrity":{"algorithm":"sha256","digest":")" +
      wise::integrity::sha256_hex(malformed) + R"("}})";
  payload.clear();
  if (wise::integrity::verify_v2(malformed_document, &payload) || !payload.empty()) return 1;

  std::cout << "integrity v2: all tests passed\n";
}
