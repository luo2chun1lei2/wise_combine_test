#include "wise.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace wct {

namespace {

std::string trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(b, e - b);
}

std::vector<std::string> split_ws(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    std::string token;
    while (in >> token) {
        out.push_back(token);
    }
    return out;
}

std::vector<std::string> split_commas(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!trim(cur).empty() || !s.empty()) {
        out.push_back(trim(cur));
    }
    return out;
}

bool starts_with(const std::string& s, const std::string& p) {
    return s.rfind(p, 0) == 0;
}

std::string base_name(const std::string& s) {
    const std::size_t open = s.find('(');
    return open == std::string::npos ? s : trim(s.substr(0, open));
}

struct CountConstraint {
    std::string func;
    std::string op;
    long value = 0;
};

bool parse_count_constraint(const std::string& expr, CountConstraint& out,
                            std::string& err) {
    const std::string t = trim(expr);
    const std::string prefix = "count(";
    if (!starts_with(t, prefix)) {
        err = "constraint must start with count(";
        return false;
    }
    const std::size_t close = t.find(')', prefix.size());
    if (close == std::string::npos) {
        err = "constraint missing ')'";
        return false;
    }
    out.func = trim(t.substr(prefix.size(), close - prefix.size()));
    if (out.func.empty()) {
        err = "constraint function name is empty";
        return false;
    }
    const std::string rest = trim(t.substr(close + 1));
    const auto tokens = split_ws(rest);
    if (tokens.size() != 2) {
        err = "constraint must be: count(<func>) <op> <number>";
        return false;
    }
    out.op = tokens[0];
    const std::set<std::string> valid_ops{"<", "<=", ">", ">=", "==", "!="};
    if (!valid_ops.count(out.op)) {
        err = "unknown constraint operator: " + out.op;
        return false;
    }
    try {
        out.value = std::stol(tokens[1]);
    } catch (...) {
        err = "invalid constraint number: " + tokens[1];
        return false;
    }
    return true;
}

std::string now_string() {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

} // namespace

Parser::Parser(std::string file) : file_(std::move(file)) {}

std::string Parser::peek_line() const {
    return pos_ < lines_.size() ? lines_[pos_] : std::string();
}

std::string Parser::next_line() {
    return pos_ < lines_.size() ? lines_[pos_++] : std::string();
}

void Parser::fail(int line, const std::string& message) const {
    throw ParseError{file_, line, message};
}

void Parser::skip_empty_or_comment() {
    while (pos_ < lines_.size()) {
        const std::string t = trim(lines_[pos_]);
        if (t.empty() || starts_with(t, "#")) {
            ++pos_;
        } else {
            break;
        }
    }
}

Spec Parser::parse() {
    std::ifstream in(file_);
    if (!in) {
        throw ParseError{file_, 0, "cannot open file: " + file_};
    }
    std::string line;
    while (std::getline(in, line)) {
        lines_.push_back(line);
    }

    Spec spec;
    while (pos_ < lines_.size()) {
        skip_empty_or_comment();
        if (pos_ >= lines_.size()) {
            break;
        }
        const int line_no = static_cast<int>(pos_ + 1);
        const std::string text = trim(lines_[pos_]);
        if (starts_with(text, "object ")) {
            spec.objects.push_back(parse_object(line_no));
        } else if (starts_with(text, "function ")) {
            spec.functions.push_back(parse_function(text, line_no));
            ++pos_;
        } else if (starts_with(text, "parameter ")) {
            parse_parameter(text, line_no, spec);
            ++pos_;
        } else if (starts_with(text, "order ")) {
            parse_order(text, line_no, spec);
            ++pos_;
        } else if (starts_with(text, "mutex ")) {
            parse_mutex(text, line_no, spec);
            ++pos_;
        } else if (starts_with(text, "constraint ")) {
            parse_constraint(text, line_no, spec);
            ++pos_;
        } else if (text == "}") {
            fail(line_no, "unexpected closing brace");
        } else {
            fail(line_no, "unrecognized top-level statement: " + text);
        }
    }
    return spec;
}

ObjectDecl Parser::parse_object(int line) {
    const std::string text = trim(lines_[pos_]);
    const std::string rest = trim(text.substr(std::string("object").size()));
    const std::size_t brace = rest.find('{');
    if (brace == std::string::npos) {
        fail(line, "expected '{' after object name");
    }
    const std::string name = trim(rest.substr(0, brace));
    if (name.empty()) {
        fail(line, "object name is empty");
    }
    ObjectDecl object;
    object.name = name;
    object.line = line;
    ++pos_;
    parse_object_body(object);
    return object;
}

void Parser::parse_object_body(ObjectDecl& object) {
    std::unordered_set<std::string> seen_states;
    while (pos_ < lines_.size()) {
        const int line = static_cast<int>(pos_ + 1);
        const std::string text = trim(lines_[pos_]);
        if (text.empty() || starts_with(text, "#")) {
            ++pos_;
            continue;
        }
        if (text == "}") {
            ++pos_;
            return;
        }
        const auto tokens = split_ws(text);
        if (tokens.empty()) {
            ++pos_;
            continue;
        }
        if (tokens[0] == "state") {
            if (tokens.size() < 2) {
                fail(line, "state requires a name");
            }
            StateDecl state;
            state.name = tokens[1];
            state.line = line;
            for (std::size_t i = 2; i < tokens.size(); ++i) {
                if (tokens[i] == "initial") {
                    state.initial = true;
                } else if (tokens[i] == "final") {
                    state.final = true;
                } else {
                    fail(line, "unknown state modifier: " + tokens[i]);
                }
            }
            if (!seen_states.insert(state.name).second) {
                fail(line, "duplicate state: " + state.name);
            }
            object.states.push_back(state);
            ++pos_;
        } else if (tokens[0] == "transition") {
            TransitionDecl tr;
            tr.line = line;
            std::size_t i = 1;
            if (i >= tokens.size()) {
                fail(line, "transition missing source state");
            }
            tr.src = tokens[i++];
            if (i >= tokens.size() || tokens[i] != "->") {
                fail(line, "transition missing '->'");
            }
            ++i;
            if (i >= tokens.size()) {
                fail(line, "transition missing target state");
            }
            tr.dst = tokens[i++];
            if (i >= tokens.size() || tokens[i] != "by") {
                fail(line, "transition missing 'by'");
            }
            ++i;
            if (i >= tokens.size()) {
                fail(line, "transition missing function");
            }
            tr.func = base_name(tokens[i++]);
            if (i < tokens.size() && tokens[i] == "guard") {
                ++i;
                std::ostringstream g;
                while (i < tokens.size()) {
                    if (!g.str().empty()) {
                        g << ' ';
                    }
                    g << tokens[i++];
                }
                tr.guard = g.str();
            } else if (i < tokens.size()) {
                fail(line, "unexpected transition token: " + tokens[i]);
            }
            object.transitions.push_back(tr);
            ++pos_;
        } else {
            fail(line, "unrecognized object statement: " + text);
        }
    }
    fail(object.line, "unterminated object block");
}

FunctionDecl Parser::parse_function(const std::string& text, int line) {
    const std::string rest = trim(text.substr(std::string("function").size()));
    const std::size_t open = rest.find('(');
    if (open == std::string::npos) {
        fail(line, "function declaration missing '('");
    }
    const std::size_t close = rest.find(')', open);
    if (close == std::string::npos) {
        fail(line, "function declaration missing ')'");
    }
    FunctionDecl fn;
    fn.name = trim(rest.substr(0, open));
    fn.line = line;
    if (fn.name.empty()) {
        fail(line, "function name is empty");
    }
    const std::string params = rest.substr(open + 1, close - open - 1);
    if (!trim(params).empty()) {
        for (const auto& part : split_commas(params)) {
            if (part.empty()) {
                continue;
            }
            const auto tokens = split_ws(part);
            Param p;
            if (tokens.size() == 1) {
                p.name = tokens[0];
            } else if (tokens.size() == 2) {
                p.type = tokens[0];
                p.name = tokens[1];
            } else {
                fail(line, "invalid parameter declaration: " + part);
            }
            fn.params.push_back(p);
        }
    }
    const std::size_t arrow = rest.find("->", close);
    if (arrow != std::string::npos) {
        fn.return_param = trim(rest.substr(arrow + 2));
        if (fn.return_param.empty()) {
            fail(line, "return parameter is empty");
        }
    }
    return fn;
}

void Parser::parse_parameter(const std::string& text, int line, Spec& spec) {
    const std::string rest = trim(text.substr(std::string("parameter").size()));
    const std::size_t eq = rest.find('=');
    if (eq == std::string::npos) {
        fail(line, "parameter relation missing '='");
    }
    const std::string lhs = trim(rest.substr(0, eq));
    const std::string rhs = trim(rest.substr(eq + 1));
    const std::size_t ldot = lhs.find('.');
    const std::size_t rdot = rhs.find('.');
    if (ldot == std::string::npos || rdot == std::string::npos) {
        fail(line, "parameter relation must use func.param format");
    }
    ParameterRel rel;
    rel.line = line;
    rel.lhs_func = trim(lhs.substr(0, ldot));
    rel.lhs_param = trim(lhs.substr(ldot + 1));
    rel.rhs_func = trim(rhs.substr(0, rdot));
    rel.rhs_param = trim(rhs.substr(rdot + 1));
    spec.parameters.push_back(rel);
}

void Parser::parse_order(const std::string& text, int line, Spec& spec) {
    const auto tokens = split_ws(trim(text.substr(std::string("order").size())));
    if (tokens.size() != 3 || tokens[1] != "before") {
        fail(line, "order must be: order <a> before <b>");
    }
    OrderRel rel;
    rel.line = line;
    rel.before = tokens[0];
    rel.after = tokens[2];
    spec.orders.push_back(rel);
}

void Parser::parse_mutex(const std::string& text, int line, Spec& spec) {
    const auto tokens = split_ws(trim(text.substr(std::string("mutex").size())));
    if (tokens.size() != 2) {
        fail(line, "mutex must list exactly two functions");
    }
    MutexRel rel;
    rel.line = line;
    rel.a = tokens[0];
    rel.b = tokens[1];
    spec.mutexes.push_back(rel);
}

void Parser::parse_constraint(const std::string& text, int line, Spec& spec) {
    const std::string expr = trim(text.substr(std::string("constraint").size()));
    if (expr.empty()) {
        fail(line, "constraint expression is empty");
    }
    ConstraintRel rel;
    rel.line = line;
    rel.expr = expr;
    spec.constraints.push_back(rel);
}

Model::Model(Spec spec) : spec_(std::move(spec)) {
    for (std::size_t i = 0; i < spec_.functions.size(); ++i) {
        const std::string& name = spec_.functions[i].name;
        if (!function_index_.emplace(name, i).second) {
            throw ModelError{"duplicate function: " + name};
        }
    }
    for (std::size_t i = 0; i < spec_.objects.size(); ++i) {
        const std::string& name = spec_.objects[i].name;
        if (!object_index_.emplace(name, i).second) {
            throw ModelError{"duplicate object: " + name};
        }
    }
}

void Model::validate() {
    for (const auto& object : spec_.objects) {
        std::size_t initial_count = 0;
        std::unordered_set<std::string> states;
        for (const auto& state : object.states) {
            if (!states.insert(state.name).second) {
                throw ModelError{"duplicate state in object " + object.name +
                                 ": " + state.name};
            }
            if (state.initial) {
                ++initial_count;
            }
        }
        if (initial_count != 1) {
            throw ModelError{"object " + object.name +
                             " must have exactly one initial state"};
        }
        for (const auto& tr : object.transitions) {
            if (!states.count(tr.src) || !states.count(tr.dst)) {
                throw ModelError{"transition references unknown state in " +
                                 object.name + ": " + tr.src + " -> " + tr.dst};
            }
            if (!has_function(tr.func)) {
                throw ModelError{"transition references unknown function: " +
                                 tr.func};
            }
        }
    }

    for (const auto& rel : spec_.parameters) {
        if (!has_function(rel.lhs_func) || !has_function(rel.rhs_func)) {
            throw ModelError{"parameter relation references unknown function"};
        }
        const FunctionDecl& lhs = function(rel.lhs_func);
        bool lhs_ok = false;
        for (const auto& p : lhs.params) {
            if (p.name == rel.lhs_param) {
                lhs_ok = true;
                break;
            }
        }
        if (!lhs_ok && lhs.return_param != rel.lhs_param) {
            throw ModelError{"unknown parameter " + rel.lhs_func + "." +
                             rel.lhs_param};
        }
        const FunctionDecl& rhs = function(rel.rhs_func);
        if (rhs.return_param != rel.rhs_param) {
            bool rhs_ok = false;
            for (const auto& p : rhs.params) {
                if (p.name == rel.rhs_param) {
                    rhs_ok = true;
                    break;
                }
            }
            if (!rhs_ok) {
                throw ModelError{"unknown source parameter " + rel.rhs_func +
                                 "." + rel.rhs_param};
            }
        }
    }

    std::unordered_map<std::string, std::size_t> indeg;
    for (const auto& fn : spec_.functions) {
        indeg[fn.name] = 0;
    }
    std::unordered_map<std::string, std::vector<std::string>> edges;
    for (const auto& rel : spec_.orders) {
        if (!has_function(rel.before) || !has_function(rel.after)) {
            throw ModelError{"order references unknown function"};
        }
        edges[rel.before].push_back(rel.after);
        ++indeg[rel.after];
    }
    std::vector<std::string> ready;
    for (const auto& [name, deg] : indeg) {
        if (deg == 0) {
            ready.push_back(name);
        }
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        const std::string name = ready.back();
        ready.pop_back();
        ++visited;
        for (const auto& nxt : edges[name]) {
            if (--indeg[nxt] == 0) {
                ready.push_back(nxt);
            }
        }
    }
    if (visited != spec_.functions.size()) {
        throw ModelError{"order relations contain a cycle"};
    }

    for (const auto& rel : spec_.mutexes) {
        if (!has_function(rel.a) || !has_function(rel.b)) {
            throw ModelError{"mutex references unknown function"};
        }
    }
    for (const auto& rel : spec_.constraints) {
        CountConstraint cc;
        std::string err;
        if (!parse_count_constraint(rel.expr, cc, err)) {
            throw ModelError{"invalid constraint: " + err};
        }
        if (!has_function(cc.func)) {
            throw ModelError{"constraint references unknown function: " + cc.func};
        }
    }
}

const FunctionDecl& Model::function(const std::string& name) const {
    const auto it = function_index_.find(name);
    if (it == function_index_.end()) {
        throw ModelError{"function not found: " + name};
    }
    return spec_.functions[it->second];
}

bool Model::has_function(const std::string& name) const {
    return function_index_.count(name) != 0;
}

const ObjectDecl& Model::object(const std::string& name) const {
    const auto it = object_index_.find(name);
    if (it == object_index_.end()) {
        throw ModelError{"object not found: " + name};
    }
    return spec_.objects[it->second];
}

Generator::Generator(const Model& model, const GenerationOptions& options)
    : model_(model), options_(options) {}

std::vector<Flow> Generator::generate_state_flows() const {
    std::vector<Flow> out;
    for (const auto& object : model_.spec().objects) {
        std::size_t initial = 0;
        bool found = false;
        for (std::size_t i = 0; i < object.states.size(); ++i) {
            if (object.states[i].initial) {
                initial = i;
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }
        std::vector<std::string> path;
        std::unordered_map<std::string, std::size_t> visits;
        state_dfs(object, initial, path, visits, out);
    }
    return out;
}

void Generator::state_dfs(const ObjectDecl& object, std::size_t state_index,
                          std::vector<std::string>& path,
                          std::unordered_map<std::string, std::size_t>& visits,
                          std::vector<Flow>& out) const {
    if (out.size() >= options_.max_flows) {
        options_.truncated = true;
        return;
    }
    const StateDecl& state = object.states[state_index];
    std::size_t& visit = visits[state.name];
    if (visit >= options_.max_state_visits) {
        return;
    }
    ++visit;
    if (state.final && !path.empty()) {
        out.push_back(path);
    }
    if (path.size() >= options_.max_depth) {
        --visit;
        return;
    }
    for (const auto& tr : object.transitions) {
        if (tr.src != state.name) {
            continue;
        }
        std::size_t next = object.states.size();
        for (std::size_t i = 0; i < object.states.size(); ++i) {
            if (object.states[i].name == tr.dst) {
                next = i;
                break;
            }
        }
        if (next == object.states.size()) {
            continue;
        }
        path.push_back(tr.func);
        state_dfs(object, next, path, visits, out);
        path.pop_back();
        if (out.size() >= options_.max_flows) {
            options_.truncated = true;
        }
    }
    --visit;
}

std::vector<Flow> Generator::generate_function_flows() const {
    std::vector<Flow> out;
    std::map<std::string, std::size_t> indeg;
    for (const auto& fn : model_.spec().functions) {
        indeg[fn.name] = 0;
    }
    for (const auto& rel : model_.spec().orders) {
        ++indeg[rel.after];
    }
    std::vector<std::string> current;
    topo_enumerate(current, indeg, out);
    return out;
}

void Generator::topo_enumerate(std::vector<std::string>& current,
                               std::map<std::string, std::size_t>& indeg,
                               std::vector<Flow>& out) const {
    if (out.size() >= options_.max_flows) {
        options_.truncated = true;
        return;
    }
    if (current.size() == model_.spec().functions.size()) {
        if (order_respected(current) && !mutex_violated(current) &&
            !constraint_violated(current)) {
            out.push_back(current);
            if (out.size() >= options_.max_flows) {
                options_.truncated = true;
            }
        }
        return;
    }
    for (const auto& [name, deg] : indeg) {
        if (deg != 0) {
            continue;
        }
        current.push_back(name);
        indeg[name] = static_cast<std::size_t>(-1);
        for (const auto& rel : model_.spec().orders) {
            if (rel.before == name) {
                --indeg[rel.after];
            }
        }
        topo_enumerate(current, indeg, out);
        for (const auto& rel : model_.spec().orders) {
            if (rel.before == name) {
                ++indeg[rel.after];
            }
        }
        indeg[name] = 0;
        current.pop_back();
        if (out.size() >= options_.max_flows) {
            options_.truncated = true;
            return;
        }
    }
}

bool Generator::order_respected(const Flow& flow) const {
    std::unordered_map<std::string, std::size_t> pos;
    for (std::size_t i = 0; i < flow.size(); ++i) {
        pos[flow[i]] = i;
    }
    for (const auto& rel : model_.spec().orders) {
        if (pos[rel.before] >= pos[rel.after]) {
            return false;
        }
    }
    return true;
}

bool Generator::mutex_violated(const Flow& flow) const {
    std::unordered_set<std::string> present(flow.begin(), flow.end());
    for (const auto& rel : model_.spec().mutexes) {
        if (present.count(rel.a) && present.count(rel.b)) {
            return true;
        }
    }
    return false;
}

bool Generator::constraint_violated(const Flow& flow) const {
    std::unordered_map<std::string, std::size_t> counts;
    for (const auto& name : flow) {
        ++counts[name];
    }
    for (const auto& rel : model_.spec().constraints) {
        CountConstraint cc;
        std::string err;
        if (!parse_count_constraint(rel.expr, cc, err)) {
            continue;
        }
        const long actual = static_cast<long>(counts[cc.func]);
        bool ok = false;
        if (cc.op == "<") {
            ok = actual < cc.value;
        } else if (cc.op == "<=") {
            ok = actual <= cc.value;
        } else if (cc.op == ">") {
            ok = actual > cc.value;
        } else if (cc.op == ">=") {
            ok = actual >= cc.value;
        } else if (cc.op == "==") {
            ok = actual == cc.value;
        } else if (cc.op == "!=") {
            ok = actual != cc.value;
        }
        if (!ok) {
            return true;
        }
    }
    return false;
}

Runner::Runner(const RunnerOptions& options) : options_(options) {}

std::vector<FlowResult> Runner::run(const std::vector<Flow>& flows) const {
    std::vector<FlowResult> results;
    results.reserve(flows.size());
    for (const auto& flow : flows) {
        if (options_.dry_run) {
            results.push_back(run_not_executed(flow));
        } else {
            results.push_back(run_direct(flow));
        }
    }
    return results;
}

FlowResult Runner::run_not_executed(const Flow& flow) const {
    FlowResult r;
    r.flow = flow;
    r.status = "not_executed";
    r.detail = "dry-run: no library loaded";
    return r;
}

FlowResult Runner::run_direct(const Flow& flow) const {
    FlowResult result;
    result.flow = flow;
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        result.status = "failed";
        result.detail = "pipe failed";
        return result;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.status = "failed";
        result.detail = "fork failed";
        return result;
    }
    if (pid == 0) {
        close(pipefd[0]);
        alarm(options_.timeout_seconds > 0 ? options_.timeout_seconds : 1);
        void* handle = dlopen(options_.lib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            const char* raw = dlerror();
            const std::string err = raw ? raw : "unknown dlopen error";
            const char* msg = err.c_str();
            const ssize_t ignored1 = write(pipefd[1], msg, err.size());
            (void)ignored1;
            close(pipefd[1]);
            _exit(127);
        }
        for (const auto& name : flow) {
            dlerror();
            using Fn = int (*)();
            const Fn fn = reinterpret_cast<Fn>(dlsym(handle, name.c_str()));
            const char* err = dlerror();
            if (err) {
                const std::string msg = std::string("symbol not found: ") + name + ": " + err;
                const ssize_t ignored2 = write(pipefd[1], msg.c_str(), msg.size());
                (void)ignored2;
                dlclose(handle);
                close(pipefd[1]);
                _exit(126);
            }
            const int ret = fn();
            if (ret != 0) {
                const std::string msg = "function returned non-zero: " + name + " (" + std::to_string(ret) + ")";
                const ssize_t ignored3 = write(pipefd[1], msg.c_str(), msg.size());
                (void)ignored3;
                dlclose(handle);
                close(pipefd[1]);
                _exit(1);
            }
        }
        dlclose(handle);
        close(pipefd[1]);
        _exit(0);
    }
    close(pipefd[1]);
    std::string detail;
    char buf[512];
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        detail.append(buf, static_cast<std::size_t>(n));
    }
    close(pipefd[0]);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        if (result.exit_code == 0) {
            result.status = "passed";
        } else {
            result.status = "failed";
        }
    } else if (WIFSIGNALED(status)) {
        result.exit_code = WTERMSIG(status);
        if (WTERMSIG(status) == SIGALRM) {
            result.status = "timeout";
            result.detail = "timed out";
        } else {
            result.status = "crashed";
            result.detail = "signal " + std::to_string(WTERMSIG(status));
        }
    }
    if (!detail.empty()) {
        result.detail = detail;
    }
    return result;
}

std::string Runner::generate_standalone(const std::vector<Flow>& flows) const {
    std::ostringstream out;
    out << "#include <cstdio>\n";
    out << "#include <cstdlib>\n\n";
    std::set<std::string> functions;
    for (const auto& flow : flows) {
        functions.insert(flow.begin(), flow.end());
    }
    for (const auto& name : functions) {
        out << "extern \"C\" int " << name << "();\n";
    }
    out << "\n";
    for (std::size_t i = 0; i < flows.size(); ++i) {
        out << "static int run_flow_" << i << "() {\n";
        for (const auto& name : flows[i]) {
            out << "  if (" << name << "() != 0) return 1;\n";
        }
        out << "  return 0;\n";
        out << "}\n\n";
    }
    out << "int main() {\n";
    out << "  int failures = 0;\n";
    for (std::size_t i = 0; i < flows.size(); ++i) {
        out << "  int r" << i << " = run_flow_" << i << "();\n";
        out << "  std::printf(\"flow " << i << " %s\\n\", r" << i << " == 0 ? \"passed\" : \"failed\");\n";
        out << "  failures += r" << i << ";\n";
    }
    out << "  return failures == 0 ? 0 : 1;\n";
    out << "}\n";
    return out.str();
}

Logger::Logger(const LogOptions& options) : options_(options) {}

void Logger::rotate_if_needed() {
    struct stat st{};
    if (stat(options_.file.c_str(), &st) != 0) {
        return;
    }
    if (static_cast<std::size_t>(st.st_size) < options_.max_size) {
        return;
    }
    for (std::size_t i = options_.rotate_count; i > 0; --i) {
        const std::string src = options_.file + (i == 1 ? "" : "." + std::to_string(i - 1));
        const std::string dst = options_.file + "." + std::to_string(i);
        if (i == 1) {
            rename(src.c_str(), dst.c_str());
        } else {
            rename(src.c_str(), dst.c_str());
        }
    }
}

void Logger::log(const std::string& level, const std::string& module,
                 const std::string& flow, const std::string& message) {
    rotate_if_needed();
    std::ofstream out(options_.file, std::ios::app);
    if (out) {
        out << "[" << now_string() << "][" << level << "][" << module
            << "][" << flow << "] " << message << "\n";
    }
}

std::string flow_id(const Flow& flow) {
    std::ostringstream out;
    for (std::size_t i = 0; i < flow.size(); ++i) {
        if (i) {
            out << "->";
        }
        out << flow[i];
    }
    return out.str();
}

std::string render_report(const std::vector<FlowResult>& results,
                          const std::string& format) {
    std::size_t passed = 0;
    std::size_t failed = 0;
    for (const auto& r : results) {
        if (r.status == "passed") {
            ++passed;
        } else if (r.status != "not_executed") {
            ++failed;
        }
    }
    if (format == "json") {
        std::ostringstream out;
        out << "{\n";
        out << "  \"total\": " << results.size() << ",\n";
        out << "  \"passed\": " << passed << ",\n";
        out << "  \"failed\": " << failed << ",\n";
        out << "  \"flows\": [\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            out << "    {\"id\": \"" << flow_id(r.flow)
                << "\", \"status\": \"" << r.status
                << "\", \"exit_code\": " << r.exit_code
                << ", \"detail\": \"" << r.detail << "\"}";
            if (i + 1 < results.size()) {
                out << ",";
            }
            out << "\n";
        }
        out << "  ]\n";
        out << "}\n";
        return out.str();
    }
    std::ostringstream out;
    out << "total=" << results.size() << " passed=" << passed
        << " failed=" << failed << "\n";
    for (const auto& r : results) {
        out << "[" << r.status << "] " << flow_id(r.flow);
        if (!r.detail.empty()) {
            out << " (" << r.detail << ")";
        }
        out << "\n";
    }
    return out.str();
}

} // namespace wct
