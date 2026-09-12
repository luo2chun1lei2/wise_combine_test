# 需求追踪矩阵

状态含义：`已验证` 表示有实现和回归证据；`基础完成` 表示核心路径已实现但仍有扩展缺口；`待增强` 表示评审已提出但尚未实现；`明确非目标` 表示当前范围排除。

| 编号 | 需求 | 实现位置 | 测试/证据 | 状态 |
| --- | --- | --- | --- | --- |
| CT-001 | 描述状态图和迁移函数 | `src/model`, `src/spec` | model/spec/generator CTest | 已验证 |
| CT-002 | 描述 typed function 和参数来源 | `src/model/model.cpp` | `rejects_missing_parameter`, relation test | 基础完成 |
| CT-003 | 描述 argument/before 关系 | `src/model`, `src/generate` | relation and ordering tests | 已验证 |
| CT-004 | 有界、可复现流程生成 | `src/generate` | determinism/limits/seed/cycle tests | 基础完成 |
| CT-005 | adapter 隔离执行 | `src/runtime` | pass/mismatch/error/timeout/crash/cap tests | 已验证 |
| CT-006 | 协议合法性和错误分类 | `src/runtime/runtime.cpp` | malformed/extra/unknown-status tests | 基础完成 |
| CT-007 | 报告写入失败稳定退出 | `src/cli`, `src/report` | `cli_reports_error`, 22/22 suites | 基础完成 |
| CT-008 | 失败报告独立 replay 和篡改校验 | 待实现 | 待添加 | 待增强 |
| CT-009 | 完整 JSON 响应语法解析 | `src/spec/spec.cpp::parse_adapter_response` | runtime malformed/extra/duplicate/formatted 回归 | 基础完成 |
| CT-010 | Q1–Q6 长期回归入口 | `tests/evaluation`, `evaluation_q1`…`evaluation_q6` | clean/matching-mutant exit-code 回归 | 基础完成 |
| CT-011 | guard/mutex/parallel/count/value | 未冻结 | 未添加 | 明确非目标 |
| CT-012 | GUI/网络/分布式/插件 | 不适用 | 范围扫描 | 明确非目标 |
