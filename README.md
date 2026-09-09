# wise_combine_test 组合测试工具

组合测试工具用于在描述接口之间的关系后，自动生成并执行各种组合调用流程，检测对象状态迁移和函数组合调用中的问题。

## 目录结构

- `ai/`：AI 与项目相关文件，包括项目提案、设计文档和任务计划。
- `src/`：实现代码与编译构建脚本。
- `test/`：测试代码与编译构建脚本。
- `doc/`：面向客户的文档，包括 DSL 语法定义和示例文件。
- `README.md`：本文档。

## 编译

构建脚本位于 `src/`。实现完成后，在项目根目录执行构建脚本即可生成可执行文件 `wise_combine_test`。当前开发状态见 [ai/task.md](ai/task.md)。

## 测试

测试代码与构建脚本位于 `test/`。实现完成后，运行测试脚本执行单元测试、覆盖率统计和内存检查。测试与测量要求见 [ai/proposal.md](ai/proposal.md)。

## 安装

实现完成后，将生成的可执行文件 `wise_combine_test` 安装到 `PATH` 中即可使用。具体安装方式以 `src/` 中的构建脚本为准。

## 使用

1. 编写状态图与函数关系描述文件，DSL 语法定义见 [doc/dsl.md](doc/dsl.md)。
2. 参考示例文件，位于 [doc/examples/](doc/examples/)。
3. 运行组合测试：

```text
wise_combine_test <描述文件> [选项]
```

命令选项的详细说明见 [doc/dsl.md](doc/dsl.md) 及后续使用文档。

详细文档见 `doc/`。
