#include "runtime/runtime.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace wise::runtime {
namespace {
using Clock = std::chrono::steady_clock;

const model::Transition* find_transition(const model::Model& model, const std::string& id) {
  for (const auto& t : model.transitions()) if (t.id == id) return &t;
  return nullptr;
}

std::string request(const generate::Flow& flow, std::size_t step,
                    const model::Transition& t) {
  std::ostringstream out;
  out << "{\"protocol\":1,\"flow_id\":\"" << flow.flow_id << "\",\"step\":"
      << step << ",\"function\":\"" << t.function << "\",\"args\":{";
  bool first = true;
  for (const auto& [name, value] : t.args) {
    if (!first) out << ',';
    first = false;
    out << "\"" << name << "\":";
    if (value.kind == model::Scalar::Kind::string) out << "\"" << value.string_value << "\"";
    else if (value.kind == model::Scalar::Kind::boolean) out << (value.boolean_value ? "true" : "false");
    else if (value.kind == model::Scalar::Kind::integer) out << value.integer_value;
    else if (value.kind == model::Scalar::Kind::number) out << value.number_value;
    else out << "null";
  }
  return out.str() + "}}\n";
}

bool field(const std::string& text, const std::string& key, std::string& value) {
  const auto needle = "\"" + key + "\":";
  const auto pos = text.find(needle);
  if (pos == std::string::npos) return false;
  auto start = pos + needle.size();
  if (start < text.size() && text[start] == '"') {
    ++start; const auto end = text.find('"', start); if (end == std::string::npos) return false;
    value = text.substr(start, end - start); return true;
  }
  const auto end = text.find_first_of(",}", start); if (end == std::string::npos) return false;
  value = text.substr(start, end - start); return true;
}

StepResult run_step(const generate::Flow& flow, std::size_t index, const model::Transition& transition,
                    const Options& options) {
  StepResult result; result.index = index; result.transition = transition.id; result.function = transition.function;
  int in_pipe[2], out_pipe[2], err_pipe[2];
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0 || pipe(err_pipe) != 0) { result.detail = std::strerror(errno); return result; }
  const pid_t pid = fork();
  if (pid < 0) { result.detail = std::strerror(errno); return result; }
  if (pid == 0) {
    setpgid(0, 0); dup2(in_pipe[0], STDIN_FILENO); dup2(out_pipe[1], STDOUT_FILENO); dup2(err_pipe[1], STDERR_FILENO);
    close(in_pipe[0]); close(in_pipe[1]); close(out_pipe[0]); close(out_pipe[1]); close(err_pipe[0]); close(err_pipe[1]);
    if (!options.working_directory.empty()) chdir(options.working_directory.c_str());
    const char* env[] = {"PATH=/usr/bin:/bin", "LC_ALL=C", nullptr};
    char* const argv[] = {const_cast<char*>(options.executable.c_str()), nullptr};
    execve(options.executable.c_str(), argv, const_cast<char* const*>(env)); _exit(127);
  }
  close(in_pipe[0]); close(out_pipe[1]); close(err_pipe[1]);
  const auto payload = request(flow, index, transition); (void)!write(in_pipe[1], payload.data(), payload.size()); close(in_pipe[1]);
  fcntl(out_pipe[0], F_SETFL, O_NONBLOCK); fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);
  std::string output, errors; const auto deadline = Clock::now() + std::chrono::milliseconds(options.step_timeout_ms);
  bool out_open = true, err_open = true; char buffer[4096];
  while (out_open || err_open) {
    const auto now = Clock::now(); if (now >= deadline) { kill(-pid, SIGTERM); usleep(50000); kill(-pid, SIGKILL); result.status = Status::timeout; result.detail = "step timeout"; break; }
    pollfd fds[2]{{out_pipe[0], POLLIN, 0}, {err_pipe[0], POLLIN, 0}};
    const int wait_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
    if (poll(fds, 2, std::max(1, wait_ms)) < 0 && errno != EINTR) break;
    for (int i = 0; i < 2; ++i) {
      if (!(fds[i].revents & (POLLIN | POLLHUP))) continue;
      const int fd = i == 0 ? out_pipe[0] : err_pipe[0]; const ssize_t n = read(fd, buffer, sizeof(buffer));
      if (n > 0) { (i == 0 ? output : errors).append(buffer, static_cast<std::size_t>(n)); if (output.size() + errors.size() > options.output_limit) { kill(-pid, SIGKILL); result.status = Status::crashed; result.detail = "output limit exceeded"; out_open = err_open = false; } }
      else if (n == 0) { if (i == 0) out_open = false; else err_open = false; }
    }
  }
  close(out_pipe[0]); close(err_pipe[0]); int status = 0; waitpid(pid, &status, 0); result.exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : -1; result.stderr_text = errors;
  if (result.status == Status::passed || result.status == Status::launch_error) {
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { result.status = Status::crashed; result.detail = "subprocess exit failure"; }
    else { std::string protocol, state, response_status; const auto first = output.find_first_not_of(" \t\r\n"); const auto last = output.find_last_not_of(" \t\r\n"); if (first == std::string::npos || output[first] != '{' || last == std::string::npos || output[last] != '}' || !field(output, "protocol", protocol) || protocol != "1" || !field(output, "status", response_status) || !field(output, "observed_state", state)) { result.status = Status::protocol_error; result.detail = "malformed adapter response"; } else { result.observed_state = state == "null" ? "" : state; result.status = response_status == "ok" ? Status::passed : Status::mismatch; const auto expected = transition.expect_present ? transition.expect : transition.to; if (result.status == Status::passed && !expected.empty() && result.observed_state != expected) { result.status = Status::mismatch; result.detail = "state mismatch: expected " + expected + ", observed " + result.observed_state; } else if (response_status != "ok") result.detail = "adapter reported " + response_status; } }
  }
  return result;
}
}

RunResult execute(const model::Model& model, const generate::Flow& flow, const Options& options) {
  RunResult result; result.flow_id = flow.flow_id; const auto started = Clock::now();
  for (std::size_t i = 0; i < flow.transition_ids.size(); ++i) {
    if (Clock::now() - started > std::chrono::milliseconds(options.total_timeout_ms)) { result.status = Status::timeout; break; }
    const auto* transition = find_transition(model, flow.transition_ids[i]);
    if (transition == nullptr) { result.status = Status::protocol_error; result.steps.push_back({Status::protocol_error, i, flow.transition_ids[i], "", "", "", -1, "unknown transition"}); break; }
    auto step = run_step(flow, i, *transition, options); result.steps.push_back(step); if (step.status != Status::passed) { result.status = step.status; break; }
  }
  return result;
}
}
