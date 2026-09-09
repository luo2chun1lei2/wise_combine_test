#include "wise.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct CliOptions {
    std::vector<std::string> files;
    std::string mode = "direct";
    std::string lib_path;
    bool dry_run = false;
    std::string report = "text";
    wct::GenerationOptions gen;
    wct::LogOptions log;
};

void print_usage(std::ostream& out) {
    out << "usage: wise_combine_test <描述文件...> [选项]\n";
    out << "  --mode direct|standalone  执行模式（默认 direct）\n";
    out << "  --lib <path>              被测动态库路径\n";
    out << "  --dry-run                 只生成流程，不执行\n";
    out << "  --max-depth <n>           最大路径步数（默认 32）\n";
    out << "  --max-flows <n>           最大调用流程数量（默认 1000）\n";
    out << "  --log-file <path>         日志文件（默认 wise_combine_test.log）\n";
    out << "  --log-max-size <bytes>    日志文件大小上限（默认 10485760）\n";
    out << "  --log-rotate-count <n>    保留日志文件数量（默认 5）\n";
    out << "  --report text|json        报告格式（默认 text）\n";
}

std::size_t parse_size(const std::string& s, const std::string& opt) {
    try {
        return static_cast<std::size_t>(std::stoull(s));
    } catch (...) {
        throw std::runtime_error("invalid value for " + opt + ": " + s);
    }
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--mode") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--mode requires a value");
            }
            o.mode = argv[++i];
        } else if (a == "--lib") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--lib requires a value");
            }
            o.lib_path = argv[++i];
        } else if (a == "--dry-run") {
            o.dry_run = true;
        } else if (a == "--max-depth") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--max-depth requires a value");
            }
            o.gen.max_depth = parse_size(argv[++i], a);
        } else if (a == "--max-flows") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--max-flows requires a value");
            }
            o.gen.max_flows = parse_size(argv[++i], a);
        } else if (a == "--log-file") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--log-file requires a value");
            }
            o.log.file = argv[++i];
        } else if (a == "--log-max-size") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--log-max-size requires a value");
            }
            o.log.max_size = parse_size(argv[++i], a);
        } else if (a == "--log-rotate-count") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--log-rotate-count requires a value");
            }
            o.log.rotate_count = parse_size(argv[++i], a);
        } else if (a == "--report") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--report requires a value");
            }
            o.report = argv[++i];
        } else if (a == "-h" || a == "--help") {
            print_usage(std::cout);
            std::exit(0);
        } else if (!a.empty() && a[0] == '-') {
            throw std::runtime_error("unknown option: " + a);
        } else {
            o.files.push_back(a);
        }
    }
    if (o.files.empty()) {
        throw std::runtime_error("at least one description file is required");
    }
    if (o.log.file.empty()) {
        o.log.file = "wise_combine_test.log";
    }
    return o;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions o = parse_args(argc, argv);
        wct::Spec spec;
        for (const auto& file : o.files) {
            wct::Parser parser(file);
            const wct::Spec parsed = parser.parse();
            spec.objects.insert(spec.objects.end(), parsed.objects.begin(),
                                parsed.objects.end());
            spec.functions.insert(spec.functions.end(), parsed.functions.begin(),
                                  parsed.functions.end());
            spec.parameters.insert(spec.parameters.end(), parsed.parameters.begin(),
                                   parsed.parameters.end());
            spec.orders.insert(spec.orders.end(), parsed.orders.begin(),
                               parsed.orders.end());
            spec.mutexes.insert(spec.mutexes.end(), parsed.mutexes.begin(),
                                parsed.mutexes.end());
            spec.constraints.insert(spec.constraints.end(), parsed.constraints.begin(),
                                    parsed.constraints.end());
        }

        wct::Model model(std::move(spec));
        model.validate();

        wct::Generator generator(model, o.gen);
        std::vector<wct::Flow> flows = generator.generate_state_flows();
        const std::vector<wct::Flow> function_flows =
            generator.generate_function_flows();
        flows.insert(flows.end(), function_flows.begin(), function_flows.end());

        wct::Logger logger(o.log);
        logger.log("info", "main", "ALL", "generated " +
                                              std::to_string(flows.size()) +
                                              " flows");

        if (o.mode == "standalone") {
            wct::Runner runner({o.lib_path, true, 10});
            const std::string source = runner.generate_standalone(flows);
            const std::string out_file = "wise_standalone.cpp";
            std::ofstream fout(out_file);
            if (!fout) {
                throw std::runtime_error("cannot write " + out_file);
            }
            fout << source;
            fout.close();
            logger.log("info", "main", "ALL",
                       "standalone source written to " + out_file);
            std::cout << "standalone source written to " << out_file << "\n";
            return 0;
        }

        if (!o.dry_run && o.lib_path.empty()) {
            throw std::runtime_error(
                "direct mode requires --lib; use --dry-run to generate without executing");
        }

        wct::Runner runner({o.lib_path, o.dry_run, 10});
        const std::vector<wct::FlowResult> results = runner.run(flows);
        for (const auto& r : results) {
            logger.log(r.status == "passed" ? "info" : "warning", "runner",
                       wct::flow_id(r.flow), r.status + " " + r.detail);
        }
        std::cout << wct::render_report(results, o.report);
        if (generator.truncated()) {
            std::cout << "# warning: generation was truncated by --max-flows\n";
        }
        return 0;
    } catch (const wct::ParseError& e) {
        std::cerr << "parse error: " << e.file << ":" << e.line << ": "
                  << e.message << "\n";
        return 2;
    } catch (const wct::ModelError& e) {
        std::cerr << "model error: " << e.message << "\n";
        return 3;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
