# 项目治理与阶段门

本文件定义阶段出口、缺陷严重度、责任与期限、变更影响和发布回滚流程，作为 `ai/task.md`、`doc/capabilities.md` 的治理补充。

## 阶段门

| 阶段 | 出口证据 | 最低条件 |
|---|---|---|
| 需求 | `ai/proposal.md`、`doc/dsl.md`、`doc/state-machine-dsl.md` | 目标、范围、非目标、验收 oracle 冻结 |
| 设计 | `ai/design.md`、`ai/adr.md` | 接口契约、生成/执行/报告边界、失败路径和测试矩阵通过 |
| 编码 | 源码 + `make` | 核心 parser/validator/generator/runner 回归通过，无未缓解 High |
| 测试 | `make test`、`make oracle` | 正常/异常/边界/安全/故障注入证据齐全 |
| 发布 | `make ci` | 干净构建、版本兼容、回滚路径和限制说明通过 |
| 维护 | `ai/AI-USAGE.md`、`ai/index.md` | 变更影响分析、回归测试、文档同步完成 |

## 缺陷严重度与闭环

| 级别 | 含义 | 处理要求 |
|---|---|---|
| High | 阻断发布：核心契约被静默接受、跨模式语义不一致、失败传播失效、不可信执行无隔离 | 修复并补回归后才能进入下一阶段 |
| Medium | 影响正确性/可复现/可维护性 | 在发布前修复，或明确降级为已知限制并记录 owner |
| Low | 边界校验、文档或诊断不完整 | 进入 backlog，明确 owner 和期限 |

每个缺陷记录必须包含：现象、复现命令、根因、影响、负责人、期限、修复提交、回归证据和关闭结论。未关闭的 High/Medium 缺陷不得在 `doc/capabilities.md` 中标记为通过。

## 责任与期限

任务或缺陷至少应填写：

| 字段 | 说明 |
|---|---|
| owner | 负责人或负责角色 |
| deadline | 目标完成日期 |
| risk | 失败影响和概率 |
| evidence | 证明完成的命令/文件/提交 |
| re-review | 重新评审或复评条件 |

## 变更影响

以下变更必须同步更新对应资料、fixture、回归测试和兼容性说明：

- DSL/语法变更 → `doc/dsl.md`、`doc/state-machine-dsl.md`、`src/grammar/*.g4`、示例和负例。
- CLI/报告 schema 变更 → `README.md`、`doc/report-schema.md`、`test/run.sh`。
- 执行/隔离模型变更 → `ai/design.md`、`ai/adr.md`、harness 回归和 `make oracle`。

## 发布与回滚

- 发布前执行 `make ci`，确认 `make test` 和 `make oracle` 全部通过。
- 每个发布用 git tag 标记版本；回滚使用 git revert 或回到上一 tag，并重新执行 `make ci`。
- 发布说明应列出已知限制、未支持能力和阶段门结论。

