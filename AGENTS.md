# 实际项目: 组合测试(Combination Test)

## 项目 wise_combine_test

项目需求已迁移至 [ai/proposal.md](ai/proposal.md)，并以此作为需求单一来源。

相关文档：

- [ai/proposal.md](ai/proposal.md)：项目提案与需求。
- [ai/design.md](ai/design.md)：设计文档。
- [ai/task.md](ai/task.md)：任务计划。

项目工作流程：先建立 ai/proposal.md，再建立设计文档 ai/design.md，最后生成计划文档 ai/task.md。

## 文件夹布局

- `ai/`：存放 AI 与项目相关文件，包括 `proposal.md`、`design.md`、`task.md`。以后 AI 和项目相关的文件都放在这里。
- `doc/`：存放面向客户的文件，例如 DSL 的语法定义和例子文件。
- `src/`：存放实现代码和编译构建脚本。
- `test/`：存放测试代码和编译构建脚本。
- `README.md`：描述项目的编译、测试、安装和使用说明；具体和详细内容放到 `doc/` 中，并写清楚链接；必须使用中文。
