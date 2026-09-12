#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

int main(int argc, char** argv) {
  std::string line; if (!std::getline(std::cin, line)) return 2;
  const std::string mode = argc > 1 ? argv[1] : "ok";
  if (mode == "relation") {
    if (line.find("\"function\":\"produce\"") != std::string::npos) {
      std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"ready\",\"returns\":{\"token\":\"from-producer\"},\"stderr\":\"\"}\n";
    } else if (line.find("\"function\":\"consume\"") != std::string::npos &&
               line.find("\"input\":\"from-producer\"") != std::string::npos) {
      std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\"}\n";
    } else {
      std::cout << "{\"protocol\":1,\"status\":\"mismatch\",\"observed_state\":\"wrong\",\"returns\":{},\"stderr\":\"wrong consumer argument\"}\n";
    }
    return 0;
  }
  if (mode == "timeout") { std::this_thread::sleep_for(std::chrono::seconds(10)); return 0; }
  if (mode == "crash") _exit(9);
  if (mode == "error") { std::cout << "{\"protocol\":1,\"status\":\"error\",\"observed_state\":null,\"returns\":{},\"stderr\":\"adapter failed\"}\n"; return 0; }
  if (mode == "malformed") { std::cout << "not-json\n"; return 0; }
  if (mode == "extra") { std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\",\"extra\":1}\n"; return 0; }
  if (mode == "cap") { std::cout << std::string(17 * 1024 * 1024, 'x') << std::flush; return 0; }
  if (mode == "mismatch") { std::cerr << "adapter mismatch\n"; std::cout << "{\"protocol\":1,\"status\":\"mismatch\",\"observed_state\":\"wrong\",\"returns\":{},\"stderr\":\"\"}\n"; return 0; }
  if (mode == "unknown-status") { std::cout << "{\"protocol\":1,\"status\":\"unknown\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\"}\n"; return 0; }
  if (mode == "duplicate-status") { std::cout << "{\"protocol\":1,\"status\":\"ok\",\"status\":\"mismatch\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\"}\n"; return 0; }
  if (mode == "formatted") { std::cout << "{ \"stderr\": \"\", \"returns\": { }, \"observed_state\": \"done\", \"status\": \"ok\", \"protocol\": 1 }\n"; return 0; }
  if (mode == "unicode") { std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"\\u4f60\\u597d\\/\\uD83D\\uDE00\",\"returns\":{},\"stderr\":\"line\\ntext\"}\n"; return 0; }
  if (mode == "escaped") { std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{\"value\":\"quote \\\" slash \\\\ tab \\t\"},\"stderr\":\"line\\r\\n\"}\n"; return 0; }
  std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\"}\n"; return 0;
}
