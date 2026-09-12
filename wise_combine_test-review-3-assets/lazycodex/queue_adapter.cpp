#include "queue_sut.h"

#include <fstream>
#include <iostream>
#include <string>

static bool extract(const std::string& text, const std::string& key, std::string& value) {
  const std::string needle = "\"" + key + "\":\"";
  const auto start = text.find(needle);
  if (start == std::string::npos) return false;
  const auto begin = start + needle.size();
  const auto end = text.find('"', begin);
  if (end == std::string::npos) return false;
  value = text.substr(begin, end - begin);
  return true;
}

static bool restore(const std::string& path) {
  if (path.empty()) return true;
  std::ifstream in(path, std::ios::binary);
  if (!in) return true;
  in.read(reinterpret_cast<char*>(&queue_state), sizeof(queue_state));
  return in.gcount() == static_cast<std::streamsize>(sizeof(queue_state));
}

static bool save(const std::string& path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(reinterpret_cast<const char*>(&queue_state), sizeof(queue_state));
  return static_cast<bool>(out);
}

int main(int argc, char** argv) {
  std::string state;
  for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == "--state") state = argv[++i];
  std::string request;
  std::getline(std::cin, request);
  std::string function;
  if (!extract(request, "function", function) || !restore(state)) return 2;
  int result = -99;
  if (function == "q_open") result = q_open();
  else if (function == "q_push1") result = q_push1();
  else if (function == "q_push2") result = q_push2();
  else if (function == "q_pop") result = q_pop();
  else if (function == "q_peek") result = q_peek();
  else if (function == "q_size") result = q_size();
  else if (function == "q_close") result = q_close();
  else return 3;
  if (!save(state)) return 4;
  std::cout << "{\"protocol\":1,\"status\":\"ok\",\"observed_state\":\"r"
            << result << "\",\"returns\":{},\"stderr\":\"\"}\n";
  return 0;
}
