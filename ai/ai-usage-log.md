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

## 2026-09-12 补充回归与 trace/replay

- 输入资料：统一 Q1–Q6 oracle、覆盖率缺口和进程清理要求。
- 处理阶段：新增 oracle 回归、trace/replay、进程组清理测试、覆盖率口径更新。
- 验证命令：
  - `make check`
  - `make asan`
  - `make coverage`
- 提交：`d559ccf Add oracle regression, trace replay, and coverage notes`
- 模型/提示版本：未采集。
- 费用/token：未采集，尚不能计算 AI 净收益。
- 人工复核：已确认 direct/standalone 能力边界和 oracle 判定口径。

## 2026-09-12 覆盖率补测

- 输入资料：覆盖率缺口、JSON parser 负例、模型错误分支和 Runner dry-run 路径。
- 处理阶段：补充模型错误、约束运算符、trace 往返、严格 JSON 正负例和 dry-run 测试。
- 验证命令：
  - `make check`
  - `make asan`
  - `make coverage`
- 结果：行覆盖率 82.04%，分支覆盖率 79.69%。
- 模型/提示版本：未采集。
