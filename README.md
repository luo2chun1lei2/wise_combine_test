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

## 测试

```sh
make test
```

## 使用

```sh
./build/wise_combine_test <模型文件> [最大长度] [随机种子] [json]
```

示例：

```sh
./build/wise_combine_test doc/examples/file-functions.dsl 3 0
./build/wise_combine_test doc/examples/connection.dsl 3
```

模型文件是两类 DSL 之一：

- 函数调用序列 DSL：见 [doc/dsl.md](doc/dsl.md)。
- 状态机 DSL：见 [doc/state-machine-dsl.md](doc/state-machine-dsl.md)。

示例模型位于 [doc/examples](doc/examples)。

## 第三方组件

本项目使用 ANTLR 4.13.2 生成解析器，工具和 C++ 运行库源码位于 `third_party/`，遵循其 BSD 许可证。
