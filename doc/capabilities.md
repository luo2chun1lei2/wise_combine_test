# 能力状态与边界

本文件把工具能力按四类状态冻结，避免把路线图、可选扩展或明确非目标误判为缺陷。状态变更需同步 `ai/adr.md`、`doc/*.md` 和回归测试。

## 状态定义

- **Required / 核心契约**：公开文档承诺、本次交付必需、阶段门必须验证。
- **Advertised / 已声明可选扩展**：已实现并公开，但不属于最小核心交付。
- **Optional / 未冻结扩展**：文档提及但未作为当前核心承诺。
- **Unsupported / 明确非目标**：文档明确排除。

## 能力矩阵

| 能力 | 状态 | 验证方式 |
|---|---|---|
| 函数调用序列 DSL | Required | `make test` 中 `file-functions.dsl`、`string-functions.dsl` |
| 状态机 DSL（扁平 + guard + entry/exit） | Required | `make test` 中 `connection.dsl` |
| 嵌套、复合、浅/深历史、并发状态 | Advertised | `nested.dsl`、`nested-history.dsl`、`deep-history.dsl`、`concurrent.dsl` |
| guard 生成、`--events` 执行、harness 统一语义 | Required | `guard.dsl` 及 guard 相关回归 |
| DFS/BFS/random/tour 算法 | Required | `--algorithm` 相关回归 |
| `--max-length`、`--max-cases`、`--seed` | Required | CLI 上限回归 |
| `--harness`、`--dylib`、`--harness-json` | Required | harness 编译运行回归 |
| `--events` 状态机执行 | Required | `--events` 回归 |
| `--sequence` supplied-trigger | Required | `make oracle` 和 `observed.dsl --sequence` 回归 |
| `--timeout` 子进程/进程组隔离 | Required | `slow.dsl` 超时回归 |
| 函数 DSL 中的计数/集合/队列内容状态 | Unsupported | 通过 `--sequence` + adapter 桥接，见 `test/oracle/` |
| 经典参数组合测试（t-way 参数组合、覆盖数组、PICT/ACTS） | Unsupported | `ai/proposal.md` 明确非目标 |
| GUI | Unsupported | `ai/proposal.md` 明确非目标 |
| 分布式、多线程函数调用序列 | Unsupported | `ai/proposal.md` 明确非目标 |

## 阶段门现状

- 需求/设计：通过。
- 编码：通过。
- 测试：`make test` 与 `make oracle` 通过，故障注入覆盖 guard、超时、重复声明和负例。
- 发布：有条件通过；`make ci` 作为发布前统一回归入口。

