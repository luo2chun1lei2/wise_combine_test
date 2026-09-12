# AI 可延续资料索引

本索引用于让后续人工或 AI 按版本和来源定位项目资料。更新资料后同步本文件的“更新日期/适用 commit”。

| ID | 资料 | 路径 | 用途 | 更新日期 | 适用 commit/版本 |
|---|---|---|---|---|---|
| DOC-001 | 项目提案 | `ai/proposal.md` | 目标、范围、非目标、DSL 演进背景 | 2026-09-10 | 按 git log 绑定 |
| DOC-002 | 总体设计 | `ai/design.md` | parser/生成/执行/报告边界 | 2026-09-10 | 按 git log 绑定 |
| DOC-003 | 任务清单 | `ai/task.md` | 阶段任务与完成状态 | 2026-09-12 | 待提交 |
| DOC-004 | 架构决定 | `ai/adr.md` | 技术选择与边界决定 | 2026-09-12 | 待提交 |
| DOC-005 | 函数 DSL | `doc/dsl.md` | 函数模型语法 | 2026-09-10 | 待提交 |
| DOC-006 | 状态机 DSL | `doc/state-machine-dsl.md` | 状态机语法与 guard 语义 | 2026-09-12 | 待提交 |
| DOC-007 | 构建/测试命令 | `README.md`、`Makefile` | 构建、测试、CLI | 2026-09-12 | 待提交 |
| DOC-010 | JSON 报告结构 | `doc/report-schema.md` | `--json`/`--harness-json` 字段说明 | 2026-09-12 | 待提交 |
| DOC-011 | 能力状态与边界 | `doc/capabilities.md` | Required/Advertised/Optional/Unsupported 矩阵 | 2026-09-12 | 待提交 |
| DOC-012 | 项目治理与阶段门 | `doc/governance.md` | 阶段门、缺陷严重度、责任、回滚 | 2026-09-12 | 待提交 |
| DOC-013 | AI 成本与持续性 | `ai/AI-COST.md` | token/费用/人工审查/返工基线、离线替代 | 2026-09-12 | 待提交 |
| DOC-008 | AI 使用记录 | `ai/AI-USAGE.md` | AI 协助审计 | 2026-09-12 | 待提交 |
| DOC-009 | AI 安全检查表 | `ai/ai-security-checklist.md` | 提示注入、凭证、许可、升级风险 | 2026-09-12 | 待提交 |

## 使用约定

- 引用资料时优先使用 ID，并标注版本或 commit。
- 重大 DSL/CLI 变更需同步更新 DOC-005、DOC-006、DOC-007 和 DOC-003。
- 每次 AI 协助任务在 `ai/AI-USAGE.md` 留痕，并在此处补充分支证据。
