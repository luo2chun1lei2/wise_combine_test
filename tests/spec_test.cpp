#include "spec/spec.hpp"
#include <iostream>
#include <stdexcept>

using wise::spec::parse;
using wise::spec::SpecError;

const std::string valid = R"({"seed":7,"limits":{"max_steps":3,"max_cases":2,"max_subprocesses":1},"relations":[{"kind":"argument","producer":{"output":"token","transition":"start-step"},"consumer":{"arg":"input","transition":"consume-step"}},{"kind":"before","before":"start-step","after":"consume-step"}],"functions":[{"returns":[{"type":"text","name":"token"}],"params":[{"type":"text","name":"input"}],"id":"start"},{"id":"consume","params":[{"name":"input","type":"text"}],"returns":[]}],"transitions":[{"function":"start","to":"ready","from":"idle","id":"start-step"},{"id":"consume-step","from":"ready","to":"ready","function":"consume","args":{"input":"x"},"expect":{"state":"ready"}}],"initial_state":"idle","states":[{"id":"ready"},{"id":"idle"}],"version":1})";

void rejects(const std::string& input, const std::string& pointer) {
  try { static_cast<void>(parse(input)); }
  catch (const SpecError& e) {
    if (e.diagnostics().empty() || e.diagnostics().front().pointer != pointer) throw std::runtime_error("wrong diagnostic pointer: " + (e.diagnostics().empty() ? std::string("empty") : e.diagnostics().front().pointer));
    return;
  }
  throw std::runtime_error("expected rejection");
}

int main() {
  try {
    const auto doc = parse(valid);
    if (doc.canonical_json != wise::spec::normalize(valid) || doc.canonical_json.find("\"version\":1") == std::string::npos) throw std::runtime_error("normalization");
    rejects("{\"version\":1,\"states\":[],}", "");
    rejects("{\"version\":2}", "/version");
    rejects("{\"version\":1,\"states\":[],\"states\":[]}", "");
    rejects("{\"version\":1,\"states\":[{\"id\":\"a\"}],\"initial_state\":\"missing\",\"transitions\":[],\"functions\":[],\"relations\":[],\"limits\":{\"max_cases\":1,\"max_steps\":1,\"max_subprocesses\":1},\"seed\":0}", "");
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  std::cout << "spec: all tests passed\n";
}
