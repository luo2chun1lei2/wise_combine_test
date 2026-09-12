# AI 使用台账

本文档记录可提交的 AI 过程证据。敏感提示、凭证和本地会话状态不入库。

## 2026-09-12 Plan/Goal 发布阻断修复

- 输入资料：`wise_combine_test.evaluate` 中 Plan/Goal 独立复评与架构复核。
- 处理阶段：需求边界确认、设计更新、任务状态对齐、代码修复、单元测试与构建验证。
- 主要人工控制点：确认 direct/standalone 能力边界采用“仅支持无参 C ABI，带参走 adapter”。
- 验证命令：
  - `make check`
  - `make asan`
- 提交：`636ede2 Fix plan_goal release blockers from review`
- 模型/提示版本：未采集，本次不用于净收益宣称。
- 后续改进：记录模型/工具版本、调用次数、费用、人工复核时间和返工量。
