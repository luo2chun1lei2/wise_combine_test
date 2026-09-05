#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

int main(int argc, char** argv) {
  std::string line; if (!std::getline(std::cin, line)) return 2;
  const std::string mode = argc > 1 ? argv[1] : "ok";
  if (mode == "timeout") { std::this_thread::sleep_for(std::chrono::seconds(10)); return 0; }
  if (mode == "crash") _exit(9);
  if (mode == "malformed") { std::cout << "not-json\n"; return 0; }
  if (mode == "extra") { std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\",\"extra\":1}\n"; return 0; }
  if (mode == "cap") { std::cout << std::string(17 * 1024 * 1024, 'x') << std::flush; return 0; }
  if (mode == "mismatch") { std::cerr << "adapter mismatch\n"; std::cout << "{\"protocol\":1,\"status\":\"mismatch\",\"observed_state\":\"wrong\",\"returns\":{},\"stderr\":\"\"}\n"; return 0; }
  std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"done\",\"returns\":{},\"stderr\":\"\"}\n"; return 0;
}
