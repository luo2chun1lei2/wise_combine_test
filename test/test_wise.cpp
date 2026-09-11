#include "../src/wise.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct FixedStrategy : wct::GenerationStrategy {
    std::vector<wct::Flow> generate_state_flows() const override {
        return {{"f", "g"}};
    }
    std::vector<wct::Flow> generate_function_flows() const override {
        return {{"g"}};
    }
    bool truncated() const override { return false; }
};

bool throws_model(const std::string& file) {
    wct::Parser parser(file);
    wct::Spec spec = parser.parse();
    wct::Model model(std::move(spec));
    model.validate();
    return false;
}

std::string write_tmp(const std::string& content) {
    const std::string path = "/tmp/wise_test_input.ct";
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

bool expect_parse_error(const std::string& content) {
    const std::string path = write_tmp(content);
    try {
        wct::Parser parser(path);
        parser.parse();
        return false;
    } catch (const wct::ParseError&) {
        return true;
    }
}

bool expect_model_error(const std::string& content) {
    const std::string path = write_tmp(content);
    try {
        wct::Parser parser(path);
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        model.validate();
        return false;
    } catch (const wct::ModelError&) {
        return true;
    }
}

} // namespace

int main() {
    {
        wct::Parser parser("test/fixtures/valid.ct");
        wct::Spec spec = parser.parse();
        assert(spec.objects.size() == 1);
        assert(spec.functions.size() == 2);
        wct::Model model(std::move(spec));
        model.validate();

        wct::Generator generator(model);
        const auto state_flows = generator.generate_state_flows();
        assert(state_flows.size() == 1);
        assert(state_flows[0].size() == 1);
        assert(state_flows[0][0] == "f");

        const auto function_flows = generator.generate_function_flows();
        assert(function_flows.size() == 1);
        assert(function_flows[0].size() == 2);
        assert(function_flows[0][0] == "f");
        assert(function_flows[0][1] == "g");
    }

    {
        bool model_error = false;
        try {
            throws_model("test/fixtures/duplicate_initial.ct");
        } catch (const wct::ModelError&) {
            model_error = true;
        }
        assert(model_error);
    }

    {
        bool model_error = false;
        try {
            throws_model("test/fixtures/cycle.ct");
        } catch (const wct::ModelError&) {
            model_error = true;
        }
        assert(model_error);
    }

    {
        bool parse_error = false;
        try {
            wct::Parser parser("test/fixtures/syntax_error.ct");
            parser.parse();
        } catch (const wct::ParseError&) {
            parse_error = true;
        }
        assert(parse_error);
    }

    {
        wct::Spec spec;
        spec.functions.push_back({"f", {}, "r", "", 1});
        wct::Model model(std::move(spec));
        wct::Generator generator(model);
        const auto flows = generator.generate_function_flows();
        assert(flows.size() == 1);
        wct::Runner runner({"", true, 10, {}});
        const std::string code = runner.generate_standalone(flows);
        assert(code.find("extern \"C\" int f();") != std::string::npos);
        assert(code.find("run_flow_0") != std::string::npos);
    }

    {
        wct::Parser parser("test/fixtures/mutex.ct");
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        model.validate();
        wct::Generator generator(model);
        const auto flows = generator.generate_function_flows();
        assert(flows.empty());
    }

    {
        wct::Parser parser("test/fixtures/constraint_none.ct");
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        model.validate();
        wct::Generator generator(model);
        const auto flows = generator.generate_function_flows();
        assert(flows.empty());
    }

    {
        wct::Parser parser("test/fixtures/full.ct");
        wct::Spec spec = parser.parse();
        assert(spec.objects.size() == 1);
        assert(spec.objects[0].states.size() == 3);
        assert(spec.objects[0].transitions.size() == 2);
        assert(spec.functions.size() == 2);
        assert(spec.functions[1].params.size() == 1);
        assert(spec.functions[1].params[0].type == "handle");
        assert(spec.functions[1].params[0].name == "h");
        assert(spec.functions[1].return_param == "status");
        assert(spec.parameters.size() == 1);
        assert(spec.orders.size() == 1);
        assert(spec.mutexes.size() == 1);
        assert(spec.constraints.size() == 1);
    }

    {
        wct::Parser parser("test/fixtures/full.ct");
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        model.validate();
        wct::Generator generator(model);
        const auto state_flows = generator.generate_state_flows();
        assert(state_flows.size() == 1);
        assert(state_flows[0].size() == 2);
        const auto function_flows = generator.generate_function_flows();
        assert(function_flows.empty());
    }

    {
        wct::FlowResult ok;
        ok.flow = {"f", "g"};
        ok.status = "passed";
        wct::FlowResult bad;
        bad.flow = {"h"};
        bad.status = "failed";
        bad.exit_code = 1;
        bad.detail = "boom";
        const std::string text =
            wct::render_report({ok, bad}, "text");
        assert(text.find("passed=1") != std::string::npos);
        assert(text.find("failed=1") != std::string::npos);
        const std::string json =
            wct::render_report({ok, bad}, "json");
        assert(json.find("\"total\": 2") != std::string::npos);
        assert(wct::flow_id(ok.flow) == "f->g");
    }

    {
        wct::LogOptions opts;
        opts.file = "/tmp/wise_test_rotation.log";
        opts.max_size = 64;
        opts.rotate_count = 2;
        {
            wct::Logger logger(opts);
            logger.log("info", "test", "flow", "one");
            logger.log("error", "test", "flow", "two");
        }
        std::ifstream in(opts.file);
        assert(in.good());
    }

    {
        wct::Runner runner({"test/out/libtest.so", false, 10, {}});
        const auto results = runner.run({{"f", "g"}, {"h"}, {"f", "h"}});
        assert(results.size() == 3);
        assert(results[0].status == "passed");
        assert(results[1].status == "failed");
        assert(results[2].status == "failed");
    }

    {
        wct::Runner runner({"test/fixtures/missing.so", false, 10, {}});
        const auto results = runner.run({{"f"}});
        assert(results.size() == 1);
        assert(results[0].status == "failed");
    }

    {
        wct::Runner runner({"test/out/libtest.so", false, 10, {}});
        const auto results = runner.run({{"missing_symbol"}});
        assert(results.size() == 1);
        assert(results[0].status == "failed");
        assert(results[0].detail.find("symbol not found") != std::string::npos);
    }

    assert(expect_parse_error("object x { state S initial } transition A -> B by f()"));
    assert(expect_parse_error("object x state S"));
    assert(expect_parse_error("object x {\n state S initial\n transition A B by f()\n}"));
    assert(expect_parse_error("object x {\n state S initial\n transition A -> B f()\n}"));
    assert(expect_parse_error("function f("));
    assert(expect_parse_error("function f() -> "));
    assert(expect_parse_error("parameter a b"));
    assert(expect_parse_error("order a before"));
    assert(expect_parse_error("mutex a"));
    assert(expect_parse_error("constraint"));
    assert(expect_parse_error("}"));
    assert(expect_parse_error("object x {\n state S initial\n transition A -> B by f()\n"));
    assert(expect_parse_error("function f(a b c)"));

    assert(expect_model_error("function f()\nfunction f()"));
    assert(expect_parse_error("object x {\n state A initial\n state A\n}"));
    assert(expect_model_error("object x {\n state A initial\n}\nobject x {\n state A initial\n}"));
    assert(expect_model_error("object x {\n state A initial\n transition A -> B by f()\n}\nfunction f()"));
    assert(expect_model_error("object x {\n state A\n state B\n transition A -> B by f()\n}\nfunction f()"));
    assert(expect_model_error("function f()\nparameter f.x = g.y"));
    assert(expect_model_error("function f(a x)\nfunction g() -> y\nparameter f.z = g.y"));
    assert(expect_model_error("function f(a x)\nfunction g() -> z\nparameter f.x = g.y"));
    assert(expect_model_error("function f()\norder f before g"));
    assert(expect_model_error("function f()\nmutex f g"));
    assert(expect_model_error("function f()\nconstraint count(g) == 1"));
    assert(expect_model_error("function f()\nconstraint bad"));

    assert(expect_model_error(
        "function init() -> handle\n"
        "function start(int h)\n"
        "parameter start.h = init.handle"));

    assert(expect_model_error(
        "function a(t x)\n"
        "function b(t y)\n"
        "parameter a.x = b.y\n"
        "parameter b.y = a.x"));

    {
        const std::string path = write_tmp(
            "function g(config c)\n"
            "parameter g.c = \"default\"");
        wct::Parser parser(path);
        wct::Spec spec = parser.parse();
        assert(spec.parameters.size() == 1);
        assert(spec.parameters[0].rhs_is_const);
        assert(spec.parameters[0].rhs_const == "default");
        wct::Model model(std::move(spec));
        model.validate();
        wct::Generator generator(model);
        const auto flows = generator.generate_function_flows();
        assert(flows.size() == 1);
        assert(flows[0] == wct::Flow{"g"});
    }

    {
        const std::string path = write_tmp(
            "function a() -> t\n"
            "function b(t x)\n"
            "function c(t y)\n"
            "parameter b.x = a.t\n"
            "parameter c.y = b.x");
        wct::Parser parser(path);
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        model.validate();
        wct::Generator generator(model);
        const auto flows = generator.generate_function_flows();
        assert(flows.size() == 1);
        assert(flows[0] == (wct::Flow{"a", "b", "c"}));
    }

    {
        wct::GuardExpr guard;
        std::string err;
        assert(wct::parse_guard("return==0", guard, err));
        assert(guard.op == "==");
        assert(guard.value == 0);
        assert(wct::guard_satisfied(guard, 0));
        assert(!wct::guard_satisfied(guard, 1));
        assert(!wct::parse_guard("return", guard, err));
    }

    assert(expect_model_error(
        "object x {\n"
        " state A initial\n"
        " state B final\n"
        " transition A -> B by f() guard return\n"
        "}\n"
        "function f()"));

    {
        const std::string path = write_tmp(
            "function f()\n"
            "function g()\n"
            "constraint count(f) > 0 and count(g) > 0");
        wct::Parser parser(path);
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        model.validate();
        wct::Generator generator(model);
        const auto flows = generator.generate_function_flows();
        assert(flows.size() == 2);
    }

    {
        const std::string path = write_tmp(
            "function f()\n"
            "function g()\n"
            "function h()\n"
            "mutex f g h");
        wct::Parser parser(path);
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        model.validate();
        wct::Generator generator(model);
        const auto flows = generator.generate_function_flows();
        assert(flows.empty());
    }

    {
        wct::Parser parser("test/fixtures/valid.ct");
        wct::Spec spec = parser.parse();
        wct::Model model(std::move(spec));
        wct::Generator generator(model);
        generator.set_strategy(std::make_shared<FixedStrategy>());
        const auto state_flows = generator.generate_state_flows();
        assert(state_flows.size() == 1);
        assert(state_flows[0] == (wct::Flow{"f", "g"}));
        const auto function_flows = generator.generate_function_flows();
        assert(function_flows.size() == 1);
        assert(function_flows[0] == (wct::Flow{"g"}));
        assert(!generator.truncated());
    }

    std::cout << "all tests passed\n";
    return 0;
}
