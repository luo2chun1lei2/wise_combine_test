# wise_combine_test

基于模型的测试工具，支持两类测试需求：

- 函数调用序列测试：根据函数语义关系生成并执行有意义的调用序列。
- 状态机路径测试：根据状态图生成并执行不同长度和途径的路径。

详细背景见 [doc/MBT.md](doc/MBT.md)，项目目标与范围见 [ai/proposal.md](ai/proposal.md)。

## 目录结构

见 [doc/layout.md](doc/layout.md)。

## 构建

依赖：

- C++17 编译器（g++）
- GNU make
- Java（仅在重新生成 ANTLR 解析器时需要，ANTLR 工具已随仓库提供）

构建：

```sh
make
```

生成的二进制位于 `build/wise_combine_test`。

## 安装

当前版本没有单独的 `make install` 目标，可直接把生成的 `build/wise_combine_test` 复制到 `PATH` 中的目录使用：

```sh
cp build/wise_combine_test /usr/local/bin/
```

如无系统目录写入权限，可改为复制到用户目录并加入 `PATH`。

## 测试

```sh
make test
```

## 使用

```sh
./build/wise_combine_test <模型文件> [--max-length N] [--seed N] [--json] [--negative] [--coverage] [--cover] [--algorithm dfs|bfs|random|tour] [--bind enumerate|random] [--n-switch N] [--guard k=v] [--harness] [--harness-json] [--dylib] [--events e1,e2,...] [--replay N] [--max-cases N]
```

常用选项：

- `--max-length N`：生成序列或路径的最大长度，默认 3。
- `--seed N`：随机种子，默认 0。
- `--max-cases N`：最大用例数；随机算法下同时作为尝试次数。
- `--json`：输出 JSON 报告。
- `--coverage`：输出函数覆盖或状态/转换覆盖统计。
- `--cover`：输出贪心计算出的最小覆盖用例集。
- `--algorithm dfs|bfs|random|tour`：生成算法；`tour` 仅状态机。
- `--bind enumerate|random`：资源实例绑定策略（仅函数模型）。
- `--n-switch N`：输出状态机 N-switch 覆盖统计（含转换对 N=2）。
- `--guard k=v`：为状态机 `--events` 执行提供 guard 变量的取值（可重复）。
- `--negative`：生成负向函数调用序列。
- `--harness`：输出 C harness 代码而不是直接执行。
- `--harness-json`：harness 失败时输出结构化 JSON。
- `--dylib`：输出动态加载被测库的 C harness 代码。
- `--events e1,e2,...`：按给定事件序列执行状态机。
- `--replay N`：输出第 N 个函数序列的 harness，或第 N 个状态机路径及其事件序列。

示例：

```sh
./build/wise_combine_test doc/examples/file-functions.dsl --max-length 3 --seed 0
./build/wise_combine_test doc/examples/connection.dsl --max-length 3
```

模型文件是两类 DSL 之一：

- 函数调用序列 DSL：见 [doc/dsl.md](doc/dsl.md)。
- 状态机 DSL：见 [doc/state-machine-dsl.md](doc/state-machine-dsl.md)。

状态机 DSL 支持扁平、嵌套、复合、浅历史和并发状态；函数 DSL 支持 `setup` 块预创建资源实例。

示例模型位于 [doc/examples](doc/examples)。

## 第三方组件

本项目使用 ANTLR 4.13.2 生成解析器，工具和 C++ 运行库源码位于 `third_party/`，遵循其 BSD 许可证。
