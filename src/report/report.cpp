#include "report/report.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace wise::report {
namespace { const char* name(runtime::Status s) { switch (s) { case runtime::Status::passed:return "passed"; case runtime::Status::mismatch:return "mismatch"; case runtime::Status::protocol_error:return "protocol_error"; case runtime::Status::adapter_error:return "adapter_error"; case runtime::Status::timeout:return "timeout"; case runtime::Status::crashed:return "crashed"; default:return "launch_error"; } } }
std::string json(const runtime::RunResult& r) {
  auto esc = [](const std::string& s) { std::ostringstream o; for (unsigned char c : s) { switch (c) { case '"': o << "\\\""; break; case '\\': o << "\\\\"; break; case '\n': o << "\\n"; break; case '\r': o << "\\r"; break; case '\t': o << "\\t"; break; default: if (c < 0x20) o << "\\u00" << std::hex << static_cast<int>(c); else o << c; } } return o.str(); };
  std::ostringstream o; o << "{\"flow_id\":\"" << esc(r.flow_id) << "\",\"status\":\"" << name(r.status) << "\",\"steps\":[";
  for (std::size_t i=0;i<r.steps.size();++i) { if(i)o<<','; const auto&s=r.steps[i]; o<<"{\"index\":"<<s.index<<",\"transition\":\""<<esc(s.transition)<<"\",\"function\":\""<<esc(s.function)<<"\",\"status\":\""<<name(s.status)<<"\",\"observed_state\":\""<<esc(s.observed_state)<<"\",\"stderr\":\""<<esc(s.stderr_text)<<"\",\"exit_status\":"<<s.exit_status<<",\"detail\":\""<<esc(s.detail)<<"\"}"; }
  o << "]}"; return o.str();
}
std::string text(const runtime::RunResult& r) { std::ostringstream o; o << "flow " << r.flow_id << ": " << name(r.status) << '\n'; for (const auto&s:r.steps) o << "step " << s.index << " " << s.function << ": " << name(s.status) << " state=" << s.observed_state << " exit=" << s.exit_status << " " << s.detail << '\n'; return o.str(); }
bool write(const runtime::RunResult&r,const std::string&d,const std::string&id,std::string* error){
  try {
    std::filesystem::create_directories(d);
    std::ofstream json_file(std::filesystem::path(d) / (id + ".json"));
    std::ofstream text_file(std::filesystem::path(d) / (id + ".txt"));
    if (!json_file || !text_file) throw std::runtime_error("cannot write report files");
    json_file << json(r);
    text_file << text(r);
    if (!json_file || !text_file) throw std::runtime_error("cannot finish writing report files");
    return true;
  } catch (const std::exception& e) {
    if (error != nullptr) *error = e.what();
    return false;
  }
}
}
