#include "runtime/runtime.hpp"
#include "report/report.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
int main(int argc, char** argv) {
  assert(argc > 1);
  wise::model::Model m; m.add_state({"idle"}); m.add_state({"done"});
  m.add_function({"finish", {}, {}}); m.add_transition({"t1", "idle", "done", "finish", {}, true, "done"}); m.set_initial_state("idle"); m.set_limits({1,1,1});
  wise::generate::Flow flow{"t1", {"t1"}};
  wise::runtime::Options o; o.executable = argv[1]; if (argc > 2) o.arguments.emplace_back(argv[2]);
  if (argc > 3) o.step_timeout_ms = static_cast<std::size_t>(std::stoul(argv[3]));
  const auto r = wise::runtime::execute(m, flow, o);
  const std::string mode = argc > 2 ? argv[2] : "ok";
  if (mode == "ok") assert(r.status == wise::runtime::Status::passed);
  else if (mode == "mismatch") assert(r.status == wise::runtime::Status::mismatch);
  else if (mode == "malformed" || mode == "extra") assert(r.status == wise::runtime::Status::protocol_error);
  else if (mode == "timeout") assert(r.status == wise::runtime::Status::timeout);
  else if (mode == "crash" || mode == "cap") assert(r.status == wise::runtime::Status::crashed);
  const auto dir = std::filesystem::temp_directory_path() / ("wise-runtime-report-" + mode); wise::report::write(r, dir.string(), "run");
  std::cout << wise::report::text(r); return 0;
}
