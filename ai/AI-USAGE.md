# AI 使用记录模板

本文件是 AI 参与需求、设计、编码、测试或维护时的最小记录模板。每次非琐碎的 AI 协助任务完成后，追加一行记录，并保留输入资料快照、输出链接、人工批准和验证证据。

## 记录表

| 编号 | 日期 | 阶段/任务 | 模型/版本 | 输入资料 | 主要提示摘要 | 产出 | 采纳/修改/拒绝 | 人工批准 | 测试/证据 | token/成本 | 风险与回滚 |
|---|---:|---|---|---|---|---|---|---|---|---|---|
| AI-001 | 2026-09-12 | 基于评测的维护改造：guard/CLI/执行隔离/统一 oracle | codex (GPT-5)，版本未记录 | DOC-001..DOC-007、DOC-010；评测目录 `wise_combine_test.evaluate` 的 review-3 | 按评审结果修复 High/Medium 项并补齐 Q1–Q6 oracle | 提交 `6bd117c`、`85ccbaf`、`37f97c2`、`533a487` | 采纳；未引入可选 `variables:` DSL 声明 | 用户 2026-09-12 指示提交 | `make test` PASS；`make oracle` 24/24；干净构建通过 | 未记录 | git 历史可回滚；无凭证/提示注入风险 |
| AI-002 | 2026-09-12 | 能力边界、JSON case_id、examples 目录文档 | codex (GPT-5)，版本未记录 | DOC-007、DOC-010、DOC-011；`doc/layout.md`、`doc/capabilities.md` | 补齐能力状态和可复现报告字段 | 提交 `c19ba9a`、`c3c0a3a`、`5b70c8c` | 采纳 | 用户 2026-09-12 指示继续 | `make test` PASS；`make oracle` PASS | 未记录 | git 历史可回滚 |
| AI-003 | 2026-09-12 | 函数 DSL 状态变量与生成探测 | codex (GPT-5)，版本未记录 | DOC-005、DOC-006、DOC-011；`src/grammar/FunctionDsl.g4`、`test/oracle/` | 原生表达 count/value 并证明生成器自动发现 6 条流程 | 提交 `b3e0250` | 采纳 | 用户 2026-09-12 指示继续 | `make oracle` 执行 + generation probe 全通过；`native-q2` 6/6 PASS | 未记录 | git 历史可回滚；grammar 已重新生成 |
| AI-004 | 2026-09-12 | AI 成本基线与项目治理流程 | codex (GPT-5)，版本未记录 | DOC-011、DOC-013；`ai/AI-USAGE.md`、`doc/governance.md` | 建立成本台账和阶段门/缺陷闭环 | 提交 `748ddb4` | 采纳 | 用户 2026-09-12 指示继续 | `make test` PASS；`make oracle` PASS | 未记录 | git 历史可回滚 |

## 填写说明

- `输入资料` 链接到 `ai/index.md` 中的资料 ID，或给出 commit/文件路径。
- `测试/证据` 填 `make test` 结果、回归脚本、缺陷发现或失败记录；不要只写“通过”。
- `人工批准` 记录谁在什么时间批准，以及是否修改或回滚。
- `风险与回滚` 记录提示注入、凭证、许可证、模型升级或不可信代码执行的处置。
- `token/成本` 字段与本仓库 `ai/AI-COST.md` 的成本基线保持一致；真实费用和 token 从供应商导出后回填。
