#include "queue_sut.h"
#include <fstream>
#include <iostream>
#include <string>
static const char* state_path = "/tmp/wct_queue_state.bin";
static void load_state() { std::ifstream in(state_path, std::ios::binary); if (in) in.read(reinterpret_cast<char*>(&queue_state), sizeof(queue_state)); }
static void save_state() { std::ofstream out(state_path, std::ios::binary|std::ios::trunc); out.write(reinterpret_cast<const char*>(&queue_state), sizeof(queue_state)); }
int main(int argc, char** argv) {
  if (argc > 1) state_path = argv[1];
  std::string s; std::getline(std::cin,s); load_state();
  std::string fn; const std::string k="\"function\":\""; auto p=s.find(k); if(p!=std::string::npos){p+=k.size(); auto e=s.find('"',p); if(e!=std::string::npos) fn=s.substr(p,e-p);}
  int r=-99;
  if(fn.rfind("q_open",0)==0)r=q_open(); else if(fn.rfind("q_push1",0)==0)r=q_push1(); else if(fn.rfind("q_push2",0)==0)r=q_push2(); else if(fn.rfind("q_pop",0)==0)r=q_pop(); else if(fn.rfind("q_peek",0)==0)r=q_peek(); else if(fn.rfind("q_size",0)==0)r=q_size(); else if(fn.rfind("q_close",0)==0)r=q_close();
  save_state(); std::cout<<"{\"protocol\":1,\"status\":\"ok\",\"return\":"<<r<<",\"returns\":{},\"stdout\":\"\",\"stderr\":\"\"}\n"; return 0;
}
