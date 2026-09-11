#ifndef WISE_COMBINE_TEST_WISE_HPP
#define WISE_COMBINE_TEST_WISE_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wct {

struct Param {
    std::string name;
    std::string type;
};

struct FunctionDecl {
    std::string name;
    std::vector<Param> params;
    std::string return_param;
    std::string return_type;
    int line = 0;
};

struct StateDecl {
    std::string name;
    bool initial = false;
    bool final = false;
    int line = 0;
};

struct TransitionDecl {
    std::string src;
    std::string dst;
    std::string func;
    std::string guard;
    bool expect_present = false;
    long expect_return = 0;
    int line = 0;
};

struct ObjectDecl {
    std::string name;
    std::vector<StateDecl> states;
    std::vector<TransitionDecl> transitions;
    int line = 0;
};

struct ParameterRel {
    std::string lhs_func;
    std::string lhs_param;
    std::string rhs_func;
    std::string rhs_param;
    bool rhs_is_const = false;
    std::string rhs_const;
    int line = 0;
};

struct OrderRel {
    std::string before;
    std::string after;
    int line = 0;
};

struct MutexRel {
    std::vector<std::string> funcs;
    int line = 0;
};

struct ConstraintRel {
    std::string expr;
    int line = 0;
};

struct GuardExpr {
    std::string op;
    long value = 0;
};

struct Spec {
    std::vector<ObjectDecl> objects;
    std::vector<FunctionDecl> functions;
    std::vector<ParameterRel> parameters;
    std::vector<OrderRel> orders;
    std::vector<MutexRel> mutexes;
    std::vector<ConstraintRel> constraints;
};

struct ParseError {
    std::string file;
    int line;
    std::string message;
};

struct ModelError {
    std::string message;
};

// A generated combination flow is an ordered list of function names to call.
using Flow = std::vector<std::string>;

struct GenerationOptions {
    std::size_t max_depth = 32;
    std::size_t max_flows = 1000;
    std::size_t max_state_visits = 8;
    bool truncated = false;
};

class Parser {
public:
    explicit Parser(std::string file);
    Spec parse();

private:
    std::string file_;
    std::vector<std::string> lines_;
    std::size_t pos_ = 0;

    std::string peek_line() const;
    std::string next_line();
    void fail(int line, const std::string& message) const;
    void skip_empty_or_comment();
    ObjectDecl parse_object(int line);
    void parse_object_body(ObjectDecl& object);
    FunctionDecl parse_function(const std::string& text, int line);
    void parse_parameter(const std::string& text, int line, Spec& spec);
    void parse_order(const std::string& text, int line, Spec& spec);
    void parse_mutex(const std::string& text, int line, Spec& spec);
    void parse_constraint(const std::string& text, int line, Spec& spec);
};

class Model {
public:
    explicit Model(Spec spec);
    void validate();

    const Spec& spec() const { return spec_; }
    const FunctionDecl& function(const std::string& name) const;
    bool has_function(const std::string& name) const;
    const ObjectDecl& object(const std::string& name) const;

private:
    Spec spec_;
    std::unordered_map<std::string, std::size_t> function_index_;
    std::unordered_map<std::string, std::size_t> object_index_;
};

class GenerationStrategy {
public:
    virtual ~GenerationStrategy() = default;
    virtual std::vector<Flow> generate_state_flows() const = 0;
    virtual std::vector<Flow> generate_function_flows() const = 0;
    virtual bool truncated() const = 0;
};

class Generator : public GenerationStrategy {
public:
    explicit Generator(const Model& model, const GenerationOptions& options = {});

    std::vector<Flow> generate_state_flows() const override;
    std::vector<Flow> generate_function_flows() const override;
    bool truncated() const override;

    void set_strategy(std::shared_ptr<GenerationStrategy> strategy) {
        strategy_ = std::move(strategy);
    }

private:
    const Model& model_;
    mutable GenerationOptions options_;
    std::shared_ptr<GenerationStrategy> strategy_;

    void state_dfs(const ObjectDecl& object, std::size_t state_index,
                   std::vector<std::string>& path,
                   std::unordered_map<std::string, std::size_t>& visits,
                   std::vector<Flow>& out) const;
    void topo_enumerate(std::vector<std::string>& current,
                        std::map<std::string, std::size_t>& indeg,
                        std::vector<Flow>& out) const;
    bool order_respected(const Flow& flow) const;
    bool parameter_respected(const Flow& flow) const;
    bool guard_allows(const TransitionDecl& transition) const;
    bool mutex_violated(const Flow& flow) const;
    bool constraint_violated(const Flow& flow) const;
};

struct FlowResult {
    Flow flow;
    std::string status; // passed, failed, crashed, timeout, not_executed
    int exit_code = 0;
    std::string detail;
};

struct RunnerOptions {
    std::string lib_path;
    bool dry_run = false;
    int timeout_seconds = 10;
    std::unordered_map<std::string, GuardExpr> guards = {};
    std::unordered_map<std::string, std::optional<long>> expected_returns = {};
    const Spec* spec = nullptr;
};

class Runner {
public:
    explicit Runner(const RunnerOptions& options);
    std::vector<FlowResult> run(const std::vector<Flow>& flows) const;
    std::string generate_standalone(const std::vector<Flow>& flows) const;

private:
    RunnerOptions options_;
    FlowResult run_direct(const Flow& flow) const;
    FlowResult run_not_executed(const Flow& flow) const;
};

struct LogOptions {
    std::string file;
    std::string level = "info";
    std::size_t max_size = 10 * 1024 * 1024;
    std::size_t rotate_count = 5;
};

class Logger {
public:
    explicit Logger(const LogOptions& options);
    void log(const std::string& level, const std::string& module,
             const std::string& flow_id, const std::string& message);

private:
    LogOptions options_;
    void rotate_if_needed();
};

std::string render_report(const std::vector<FlowResult>& results,
                          const std::string& format);

std::string flow_id(const Flow& flow);

bool parse_guard(const std::string& text, GuardExpr& out, std::string& err);
bool guard_satisfied(const GuardExpr& guard, int return_value);

} // namespace wct

#endif
