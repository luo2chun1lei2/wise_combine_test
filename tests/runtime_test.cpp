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
  wise::runtime::Options o; o.executable = argv[1];
  auto r = wise::runtime::execute(m, flow, o); assert(r.status == wise::runtime::Status::passed);
  const auto dir = std::filesystem::temp_directory_path() / "wise-runtime-report"; wise::report::write(r, dir.string(), "run");
  std::cout << wise::report::text(r); return 0;
}
