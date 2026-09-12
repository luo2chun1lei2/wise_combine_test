#include "queue_sut.h"

#include <fstream>
#include <iostream>
#include <string>

static const char* state_path = "/tmp/wct_queue_state.bin";

static void load_state() {
    std::ifstream in(state_path, std::ios::binary);
    if (in) {
        in.read(reinterpret_cast<char*>(&queue_state), sizeof(queue_state));
    }
}

static void save_state() {
    std::ofstream out(state_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(&queue_state), sizeof(queue_state));
}

int main(int argc, char** argv) {
    if (argc > 1) state_path = argv[1];
    std::string request;
    std::getline(std::cin, request);
    load_state();

    std::string function;
    const std::string marker = "\"function\":\"";
    const std::size_t pos = request.find(marker);
    if (pos != std::string::npos) {
        const std::size_t start = pos + marker.size();
        const std::size_t end = request.find('"', start);
        if (end != std::string::npos) function = request.substr(start, end - start);
    }

    int result = -99;
    if (function.rfind("q_open", 0) == 0) result = q_open();
    else if (function.rfind("q_push1", 0) == 0) result = q_push1();
    else if (function.rfind("q_push2", 0) == 0) result = q_push2();
    else if (function.rfind("q_pop", 0) == 0) result = q_pop();
    else if (function.rfind("q_peek", 0) == 0) result = q_peek();
    else if (function.rfind("q_size", 0) == 0) result = q_size();
    else if (function.rfind("q_close", 0) == 0) result = q_close();

    save_state();
    std::cout << "{\"protocol\":1,\"status\":\"ok\",\"return\":" << result
              << ",\"returns\":{},\"stdout\":\"\",\"stderr\":\"\"}\n";
    return 0;
}
