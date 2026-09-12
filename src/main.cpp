#include "wise.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>

namespace {

struct CliOptions {
    std::vector<std::string> files;
    std::string mode = "direct";
    std::string lib_path;
    std::string adapter_path;
    std::vector<std::string> adapter_args;
    bool dry_run = false;
    std::string report = "text";
    wct::GenerationOptions gen;
    wct::LogOptions log;
};

void print_usage(std::ostream& out) {
    out << "usage: wise_combine_test <描述文件...> [选项]\n";
    out << "  --mode direct|standalone  执行模式（默认 direct）\n";
    out << "  --lib <path>              被测动态库路径\n";
    out << "  --adapter <path>          使用 adapter 进程执行（JSON 协议）\n";
    out << "  --adapter-arg <arg>       adapter 附加参数（可重复）\n";
    out << "  --dry-run                 只生成流程，不执行\n";
    out << "  --max-depth <n>           最大路径步数（默认 32）\n";
    out << "  --max-flows <n>           最大调用流程数量（默认 1000）\n";
    out << "  --seed <n>                复现实验种子（默认 0）\n";
    out << "  --log-file <path>         日志文件（默认 build/wise_combine_test.log）\n";
    out << "  --log-max-size <bytes>    日志文件大小上限（默认 10485760）\n";
    out << "  --log-rotate-count <n>    保留日志文件数量（默认 5）\n";
    out << "  --report text|json        报告格式（默认 text）\n";
}

void ensure_dir(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

std::string dir_name(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return "/";
    }
    return path.substr(0, slash);
}

std::string base_name(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string lib_link_name(const std::string& path) {
    std::string base = base_name(path);
    if (base.rfind("lib", 0) == 0) {
        base = base.substr(3);
    }
    if (base.size() > 3 && base.substr(base.size() - 3) == ".so") {
        base = base.substr(0, base.size() - 3);
    }
    return base;
}

std::string rpath_expr(const std::string& dir) {
    if (dir.empty() || dir[0] == '/') {
        return dir;
    }
    return "$ORIGIN/../" + dir;
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
        } else if (a == "--adapter") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--adapter requires a value");
            }
            o.adapter_path = argv[++i];
        } else if (a == "--adapter-arg") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--adapter-arg requires a value");
            }
            o.adapter_args.push_back(argv[++i]);
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
        } else if (a == "--seed") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--seed requires a value");
            }
            o.gen.seed = parse_size(argv[++i], a);
            o.gen.seed_set = true;
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
        o.log.file = "build/wise_combine_test.log";
    }
    return o;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions o = parse_args(argc, argv);
        ensure_dir("build");
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
            spec.parallels.insert(spec.parallels.end(), parsed.parallels.begin(),
                                  parsed.parallels.end());
            spec.constraints.insert(spec.constraints.end(), parsed.constraints.begin(),
                                    parsed.constraints.end());
            spec.state_constraints.insert(spec.state_constraints.end(),
                                          parsed.state_constraints.begin(),
                                          parsed.state_constraints.end());
            spec.value_constraints.insert(spec.value_constraints.end(),
                                          parsed.value_constraints.begin(),
                                          parsed.value_constraints.end());
        }

        wct::Model model(std::move(spec));
        model.validate();

        wct::RunnerOptions runner_options{o.lib_path, o.dry_run, 10, {}};
        runner_options.spec = &model.spec();
        for (const auto& object : model.spec().objects) {
            for (const auto& tr : object.transitions) {
                if (!tr.guard.empty()) {
                    wct::GuardExpr guard;
                    std::string err;
                    wct::parse_guard(tr.guard, guard, err);
                    runner_options.guards[tr.func] = guard;
                }
                if (tr.expect_present) {
                    runner_options.expected_returns[tr.func] = tr.expect_return;
                }
                if (tr.expect_output_present) {
                    runner_options.expected_outputs[tr.func] = tr.expect_output;
                }
            }
        }
        runner_options.adapter_path = o.adapter_path;
        runner_options.adapter_args = o.adapter_args;

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
            wct::RunnerOptions standalone_options = runner_options;
            standalone_options.dry_run = true;
            wct::Runner runner(standalone_options);
            const std::string source = runner.generate_standalone(flows);
            const std::string out_file = "build/wise_standalone.cpp";
            std::ofstream fout(out_file);
            if (!fout) {
                throw std::runtime_error("cannot write " + out_file);
            }
            fout << source;
            fout.close();
            logger.log("info", "main", "ALL",
                       "standalone source written to " + out_file);
            std::cout << "standalone source written to " << out_file << "\n";
            const std::string lib_dir = o.lib_path.empty()
                                            ? "<lib_dir>"
                                            : dir_name(o.lib_path);
            const std::string lib_name = o.lib_path.empty()
                                             ? "<lib_name>"
                                             : lib_link_name(o.lib_path);
            const std::string rpath = o.lib_path.empty()
                                          ? "<lib_dir>"
                                          : rpath_expr(lib_dir);
            std::cout << "\n编译并运行示例（请把被测库链接进来）：\n";
            std::cout << "  g++ -std=c++17 " << out_file << " -L" << lib_dir
                      << " -l" << lib_name << " -Wl,-rpath," << rpath
                      << " -o build/wise_standalone\n";
            std::cout << "  ./build/wise_standalone\n";
            std::cout << "\n使用 ASan 编译运行：\n";
            std::cout << "  g++ -std=c++17 -O1 -g -fsanitize=address "
                         "-fno-omit-frame-pointer "
                      << out_file << " -L" << lib_dir << " -l" << lib_name
                      << " -Wl,-rpath," << rpath
                      << " -o build/wise_standalone_asan\n";
            std::cout << "  ASAN_OPTIONS=detect_leaks=1 "
                         "./build/wise_standalone_asan\n";
            return 0;
        }

        if (!o.dry_run && o.lib_path.empty() && o.adapter_path.empty()) {
            throw std::runtime_error(
                "execution requires --lib or --adapter; use --dry-run to generate without executing");
        }

        wct::Runner runner(runner_options);
        const std::vector<wct::FlowResult> results = runner.run(flows);
        bool any_failure = false;
        for (const auto& r : results) {
            logger.log(r.status == "passed" ? "info" : "warning", "runner",
                       wct::flow_id(r.flow), r.status + " " + r.detail);
            if (r.status != "passed" && r.status != "not_executed") {
                any_failure = true;
            }
        }
        const wct::ReportMeta meta{o.gen.seed, o.gen.seed_set, o.files};
        std::cout << wct::render_report(results, o.report, meta);
        const bool truncated = generator.truncated();
        if (truncated) {
            std::cout << "# warning: generation was truncated by --max-flows\n";
        }
        return any_failure ? 4 : (truncated ? 5 : 0);
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
