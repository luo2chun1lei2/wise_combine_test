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
  if (mode == "malformed") { std::cout << "not-json\n"; return 0; }
  if (mode == "extra") { std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\",\"extra\":1}\n"; return 0; }
  if (mode == "cap") { std::cout << std::string(17 * 1024 * 1024, 'x') << std::flush; return 0; }
  if (mode == "mismatch") { std::cerr << "adapter mismatch\n"; std::cout << "{\"protocol\":1,\"status\":\"mismatch\",\"observed_state\":\"wrong\",\"returns\":{},\"stderr\":\"\"}\n"; return 0; }
  std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\"}\n"; return 0;
}
