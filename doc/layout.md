# 文件夹和文件的布局

## 根目录

- `README.md`：项目入口说明，描述功能、目录结构、构建、测试、安装和使用方法；详细内容放到 `doc/` 中。
- `Makefile`：负责编译、生成 ANTLR 解析器、运行自测和清理构建产物。
- `AGENTS.md`：面向 AI 协作者的约定。
- `LICENSE`：项目许可证。
- `.gitignore`：忽略构建产物、临时文件和第三方编译中间文件。

## `ai/`

存放 AI 与项目相关的文档，包括：

- `proposal.md`：项目提案和目标范围。
- `design.md`：总体设计说明。
- `task.md`：任务清单和执行状态。
- `adr.md`：架构决定记录。
- `*-checklist.md`：提案、设计、任务各阶段的评审检查表。
- `index.md`：AI 可延续资料索引，记录资料版本、来源和适用 commit。
- `AI-USAGE.md`：AI 使用记录模板，用于留痕模型、输入、产出、人工批准和成本。
- `ai-security-checklist.md`：AI 生成物的安全与质量控制检查表。

## `doc/`

存放面向用户的文档和示例，包括：

- `MBT.md`：基于模型的测试背景说明。
- `dsl.md`：函数调用序列 DSL 语法定义。
- `state-machine-dsl.md`：状态机 DSL 语法定义。
- `report-schema.md`：`--json` 和 `--harness-json` 输出字段说明。
- `capabilities.md`：能力状态与边界矩阵。
- `layout.md`：本文件，说明目录结构。
- `examples/`：可运行的 DSL 示例模型。
  - `file-functions.dsl`：libc 文件函数组示例。
  - `string-functions.dsl`：libc 字符串函数组示例。
  - `observed.dsl`：带状态观察函数的句柄示例。
  - `connection.dsl`：状态机示例。
  - `guard.dsl`：带 guard 的状态机示例。
  - `slow.dsl`：用于验证 harness 超时隔离的函数示例。

## `src/`

存放工具实现代码和 DSL 语法定义，包括：

- `main.cpp`：命令行入口、模型识别、输出报告和执行控制。
- `model.h`：函数调用序列的内部模型。
- `function_model_builder.{h,cpp}`：把函数 DSL 解析结果构建为内部模型并校验。
- `sequence_generator.{h,cpp}`：函数调用序列生成器。
- `harness_generator.{h,cpp}`：生成 C harness 代码，支持直接链接和动态库加载。
- `state_machine_model.h`：状态机内部模型。
- `state_machine_builder.{h,cpp}`：把状态机 DSL 解析结果构建为内部模型并校验。
- `state_machine_path_generator.{h,cpp}`：状态机路径生成器。
- `grammar/`：ANTLR 语法文件。
  - `FunctionDsl.g4`
  - `StateMachineDsl.g4`

## `test/`

存放工具自身测试和被测桩代码，包括：

- `run.sh`：命令行回归测试脚本，由 `make test` 调用。
- `observed_sut.c`：供状态观察、直接链接和动态库加载测试使用的最小被测实现。
- `slow_sut.c`：用于验证生成 harness 超时隔离的最小挂起实现。
- `oracle/`：统一 Q1–Q6 有状态队列 oracle，包括 `queue_sut.h`、`queue_adapter.c`、`q1.dsl`–`q6.dsl` 和 `run_oracle.sh`；由 `make oracle` 调用。

## `third_party/`

存放第三方依赖：

- `antlr-tool/antlr-4.13.2-complete.jar`：生成解析器代码使用的 ANTLR 工具。
- `antlr4-runtime/`：ANTLR 4.13.2 C++ 运行库源码。

## `build/`

构建产物目录，默认被 `.gitignore` 忽略：

- `wise_combine_test`：可执行文件。
- `gen/`：由 ANTLR 根据 `src/grammar/*.g4` 生成的 C++ 解析器代码和语法分析数据。
- `gen/.stamp`：标记 ANTLR 代码生成是否已完成。
