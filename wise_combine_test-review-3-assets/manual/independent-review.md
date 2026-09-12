# Manual 方案独立复评（第二轮）

评审对象：`wise_combine_test.manual`。评审依据：`评价标准-组合测试项目.md`、冻结实验资产 `wise_combine_test-review-2-assets/common/{README.md,cases.json,queue_sut.h}`。本报告是独立证据记录，源项目未修改。

## 结论摘要

`make test` 在评审时通过（`test/run.sh` 输出 `PASS`），但它主要验证固定示例输出和若干 happy-path。按专项标准，产品仍有两个发布阻断：生成路径不执行 guard，而生成的状态机 harness 也不执行 guard；生成的 harness 是同进程直接调用，没有超时、崩溃隔离或进程组清理。综合评分 **4.4/10**，阶段门结论为 **暂缓进入发布/维护阶段**；应先修复阻断项并补齐统一 oracle 回归。

统一 Q1–Q6 实验资产已经映射尝试到 `review-2-assets/manual/`。第一次 24 次（6 个 case × clean/mutant × 2 轮）运行不计入发现率：模型的资源状态抽象和序列生成会产生大量额外序列，clean Q2–Q5 因这些额外序列失败，结果无法证明目标序列触发了相应 mutant。随后尝试按精确序列索引生成 harness；由于项目的生成器会先枚举庞大序列，且该生成步骤自身没有 10 秒预算，脚本在生成/索引阶段被中断，没有形成可接受的 24-run 证据。该限制本身是 `[CT-测试完备性] [CT-诊断复现]` 风险，不能把失败实验包装为发现率。

## 统一 oracle 实验记录

冻结 oracle 要求每个 Qn 在 clean 下所有声明返回值成立，在对应 MUTANT=n 下至少有一个真实返回值/状态不匹配；每个 case clean 与 mutant 各运行两次，共 24 runs。

适配代码和模型保存在本目录 `q*.dsl`、`queue_adapter.c`、`run_oracle.sh`。adapter 只把队列函数签名转成模型需要的 C ABI，没有加入断言；但由于模型生成的额外序列，模型本身不能代表 cases.json 的单一调用序列。第二轮脚本启动时清理了第一轮临时目录，随后在生成阶段被中断，所以当前 `results.tsv` 只有表头；这也意味着第一轮原始日志未形成可复核的持久证据。

第一轮终端观察（未持久化，故只作诊断线索）：24/24 进程都能编译并运行，但 **0/24 可作为有效 oracle 结果**。Q1/Q6 的 clean harness 恰好通过；Q2–Q5 的 clean harness 在额外生成序列上失败；mutant 运行的非零退出不能区分“目标 mutant 被发现”和“模型额外序列本来就失败”。第二轮精确序列尝试没有完成，原因是先枚举/索引生成超出 10 秒实验预算且没有产品级生成超时。

因此本轮不报告伪造的“发现率”。可接受的发现率应在修复模型映射或增加外部 supplied-trigger 单序列入口后重新测量；当前证据是“无法验证”，不是“通过”。

## 产品层

### 已确认能力

- `[CT-表达能力]` 函数 DSL 可以声明资源、状态、参数、返回类型、前置条件、效果、成功返回值和 C 符号；见 `src/model.h:8-81`、`src/function_model_builder.cpp:79-130`。
- `[CT-生成正确性]` DFS/BFS/随机序列和状态路径生成、资源状态绑定、长度上限、负向序列、覆盖统计均有实现；见 `src/sequence_generator.cpp:62-130,141-200,343-447`、`src/state_machine_path_generator.cpp:1-202`。
- `[CT-执行可靠性]` 生成 C/C++ harness，支持直接链接和动态库符号加载；返回值与状态观察可生成断言；见 `src/harness_generator.cpp:169-337`。
- `[CT-诊断复现]` 有文本/JSON、`--replay`、seed 和覆盖输出；见 `src/main.cpp:296-369,705-767,790-804`。

### 阻断和缺口

- **[HIGH] `[CT-生成正确性] [可靠性]` guard 未进入路径生成。** `StateMachinePathGenerator::dfs` 和 BFS/随机/tour 只依据 active state 匹配转换，从不求值 `transition.guard`（`src/state_machine_path_generator.cpp:181-198`；`70-100`；`105-134`）。文档明确承认“路径生成不进行 guard 求值”（`doc/state-machine-dsl.md:109-117`），但产品目标和专项 oracle 要求约束参与生成。应在生成状态中携带 guard 环境，或明确把 guard-only models 标为不可生成并报告未覆盖原因。
- **[HIGH] `[CT-执行可靠性] [CT-隔离安全]` 生成 harness 既不执行 guard，也没有超时/崩溃隔离。** 状态机 harness 仅按 `current` 和 event 生成 if 分支（`src/harness_generator.cpp:410-448`），完全没有 guard 判断；函数 harness 直接在同一进程逐步调用 SUT（`src/harness_generator.cpp:220-337`）。无限循环、SIGSEGV 或子进程遗留会拖垮整个测试进程。应采用受控子进程/进程组、墙钟预算、信号分类、输出上限和清理策略，并让生成 harness 与 CLI runner 共用 guard/状态语义。
- **[MEDIUM] `[CT-可观测性] [效率]` `--max-cases` 对 BFS 和状态机 tour 不生效。** BFS 只按 `maxLength_` 停止（`src/sequence_generator.cpp:91-130`）；`generateTour` 也完全不看 maxCases（`src/state_machine_path_generator.cpp:137-178`）。README 却把 `--max-cases` 描述成最大用例数（`README.md:54-56`）。应在每个生成算法统一实施上限，并输出截断原因。
- **[MEDIUM] `[CT-表达能力] [CT-生成正确性]` 模型状态是粗粒度手工别名，无法直接表达冻结队列的 count/value 关系。** 例如 Q2/Q5 需要“两次 push 后按 FIFO 返回 11、22并维护 size”，DSL 只能把 `OPEN/ONE/TWO/EMPTY` 当人工阶段状态，不能表达计数、队列内容或状态变量；适配因此必须制造多个别名函数/状态。应增加整数/集合状态、返回值 producer-consumer 和参数化 oracle，或在评审中把“无法表达”单独计数。
- **[MEDIUM] `[CT-诊断复现]` 报告缺少稳定 case ID、模型 hash、命令行/环境和失败实际值。** JSON 只有模型计数、序列、错误或状态机 trace（`src/main.cpp:325-369,742-766`）；函数 harness JSON 只含 seq/step/expected，不含 actual（`src/harness_generator.cpp:296-303`）。应输出 seed、版本、模型哈希、完整参数、实际返回值、状态快照和可直接重放命令。
- **[LOW] `[CT-表达能力]` 负的 `--max-length` 被静默接受并返回空结果。** CLI 没有范围校验（`src/main.cpp:998-1000`），生成循环仅因负上限不进入；`--json` 仍返回 0。应拒绝非负整数之外的值并给出明确诊断。

## 工程资料层

### 证据

需求、设计、任务、ADR、DSL 文档和 checklist 均存在：`ai/proposal.md:1-79`、`ai/design.md:1-90`、`ai/task.md:1-73`、`ai/adr.md:1-326`、`doc/dsl.md`、`doc/state-machine-dsl.md`。ADR 记录了 DSL、执行方式、状态观测、覆盖、报告和 oracle 等设计决策（如 `ai/adr.md:88-112,138-160,188-202,266-302`），这使后续定位比无文档项目容易。

### 缺口

- `[可维护性]` 文档存在时间顺序和状态漂移：ADR-014 仍说高级状态“后续升级”，而 README/任务记录已声称嵌套、历史、并发完成；`doc/state-machine-dsl.md:121` 与 `README.md:83` 口径不一致。
- `[可测试性]` 设计声称完整执行和校验（`ai/design.md:16-28,51-66`），但没有超时/崩溃/资源泄漏/报告写入失败的验收矩阵；ADR-024 明确无观察函数时只能检查返回值和崩溃（`ai/adr.md:291-302`），没有把降级风险纳入阶段出口。
- `[可维护性]` 任务清单记录已完成 T15/T30，但没有逐项链接提交、证据、未完成风险或重新评审条件；这导致本轮很难从任务状态判断 guard 生成与 harness 语义是否真正闭合。

## 项目管理层

有任务按 parser、生成、执行、状态机、测试、文档拆分（`ai/task.md:19-47`），也有 backlog。缺少工期、人员/模型调用成本、资源预算、风险 owner、里程碑出口和变更影响分析；ADR 仅记录技术选择，没有可追踪的交付计划。统一实验预算也未在项目内形成可执行脚本：`test/run.sh` 不覆盖 24-run 方案。

评分：`[交付性] [效率]` 4/10。补救任务：为每个阶段建立出口证据表、负责人和期限；把统一 oracle、构建命令、生成预算、失败分类和结果存档纳入任务；记录需求/设计变更对 DSL、harness、fixture 和回归矩阵的影响。

## 流程治理层

项目有 ADR、proposal/design/task checklist，但没有统一的阶段门、变更审批、版本兼容矩阵、发布回滚、缺陷严重度闭环或“标准/实现/测试/证据”追踪表。`test/run.sh:89-92` 对 harness-json 结果使用 `|| true`，会吞掉测试失败；这是流程上允许红灯变绿灯的直接证据。应删除吞错，所有预期失败必须显式检查退出码和 JSON 字段，并为 DSL/CLI 变更强制更新 ADR、文档、fixture、回归测试和兼容性说明。

## 七维评分

| 维度 | 分数 | 依据 |
|---|---:|---|
| 1. 模型表达和契约 | 6 | 函数/资源/FSM/约束 DSL 完整，但无法表达冻结队列的计数和值关系，guard 语义也未贯穿生成/harness。 |
| 2. 组合生成正确性和覆盖 | 5 | DFS/BFS/随机、覆盖和负向能力存在；guard 不参与路径生成，max-cases 算法不一致。 |
| 3. 执行可靠性、隔离与安全 | 3 | 直接链接、动态库和 harness 可用，但无超时、崩溃/进程隔离、输出上限或资源清理。 |
| 4. 报告、诊断和复现 | 4 | 有 JSON、replay、seed、trace；缺实际值、模型 hash、环境和稳定 case ID。 |
| 5. 工具自身测试完备性 | 3 | `make test` 通过且有参考计数、示例和端到端 harness；没有统一 oracle、故障注入、超时和完整负例，且脚本吞错。 |
| 6. 设计、文档和后续维护 | 6 | ADR/design/proposal/task 齐全；存在互相过时的边界说明、已完成状态与代码证据脱节。 |
| 7. 项目计划、风险和流程治理 | 4 | 有任务分解和 checklist；无工期、成本、风险 owner、阶段门和变更闭环。 |
| **平均** | **4.4** | High 问题存在，不能批准发布。 |

## 阶段门和补救

- 需求阶段：**有条件通过**。目标、DSL 范围和不支持项有记录，但必须补充 guard/状态变量/失败分类和统一 oracle 的可表达性边界。
- 设计阶段：**暂缓**。生成器、CLI runner、harness 的 guard 和失败隔离契约未闭合；先补接口契约、进程模型、报告 schema 和测试矩阵。
- 编码阶段：**暂缓**。两个 High 问题未缓解；`--max-cases` 和输入范围校验应纳入回归。
- 测试阶段：**暂缓**。当前 `make test` 只能作为示例回归；24-run Q1–Q6 尚未形成有效 oracle 证据，发现率必须在单序列触发入口或可表达的状态变量支持后重跑。
- 发布/维护阶段：**不通过**。没有超时/崩溃隔离、完整可复现报告、可靠的失败传播和变更出口，不应作为通用测试工具发布。

最高优先级补救：统一 guard 语义并修复生成 harness；加入受控执行器和 10 秒预算；让 DSL 表达计数/value 或提供 supplied-trigger 单序列执行 API；补齐 24-run artifact；删除测试脚本吞错；同步 ADR/README/阶段门记录。
