# wise_combine_test 四种实现方案评审

评审对象：

- `wise_combine_test.lazycodex`：LazyCodex 编排。
- `wise_combine_test.omx`：OMX 多轮规划、审查、验证编排。
- `wise_combine_test.manual`：主要由人工描述需求并判断技术选型。
- `wise_combine_test.plan_goal`：AI 自行 plan，并以 goal/任务方式完成。

本评审由四个独立 subagent 分别审查四个目录，我再用相同的构建/测试入口复核，并按“目标匹配、灵活全面、可靠性、测试完备、AI 管理可维护性”五项评分。分数为 10 分制，重点看实际行为和可维护证据，不只看 README 的功能宣称。

## 统一验证方法和隐藏问题模型

四个方案目标都是组合测试工具：输入状态图、函数及其关系，生成有意义的调用流程，执行被测对象并发现状态、参数、顺序和失败问题。为了避免“测试数量多但没有测到组合语义”，应使用一个包含状态和调用关系的模型，例如：

- 状态：`Idle -> Ready -> Closed`，另有 `reset: Ready -> Idle`。
- 函数：`open() -> token`、`consume(token)`、`reset()`、`close()`。
- 关系：`open before consume`，`consume` 必须使用 `open` 的返回值，`close` 只能在 Ready，`reset` 可重复。
- 隐藏故障：
  1. 缺少必需参数仍被接受；
  2. 同一条非自环边需要第二次经过时，生成器错误地禁止重走；
  3. producer 返回值没有真正绑定到 consumer；
  4. guard 未满足时仍生成或执行路径；
  5. adapter 返回带转义字符、字段换序或未知状态时被误判；
  6. 超时、崩溃、部分输出、报告目录不可写时错误码不准确；
  7. standalone/harness 生成代码只生成零参数调用；
  8. 失败场景被构建脚本忽略，造成“全绿”假象。

这组问题同时检验多函数、状态、调用关系、参数传递、边界协议和测试 oracle。实际仓库中的现有测试能覆盖其中一部分，但没有一个方案完整地把上述模型作为跨方案共同 oracle。

## 1. LazyCodex 方案

### 实现与目标匹配

匹配度高。代码按 `model/spec/generate/runtime/report/cli` 分层，支持状态图、函数/参数关系、before 关系、JSON 规范、adapter 进程和失败报告。README 对退出码、限制、协议和安全边界说明很具体。

独立审查发现的实际缺口：

- 规范校验只遍历 transition 中已经出现的参数；函数声明了必需参数但 transition 没提供时仍可通过 validate。这是隐藏问题 1 的直接命中。
- 非自环 transition ID 被禁止重复，合法的 `a -> b -> a` 重走场景覆盖不足，隐藏问题 2 命中。
- `seed` 在生成逻辑中基本未参与选择，不同 seed 可能得到相同结果，与“可复现且可控随机”宣传不完全一致。
- adapter JSON 使用 substring/手写片段解析，不能稳健处理空格、字段顺序、转义、嵌套或未知字段；`status` 的未知值也可能被归为 mismatch 而不是协议错误。
- 每一步都 fork/exec，adapter 进程之间不保留状态；需要跨步骤有状态对象时只能依赖外部隐式状态。
- pipe/fork/write 失败路径和报告文件写入失败检查不足，部分错误可能静默成功。

### 灵活性、全面性和可靠性

优点是边界和安全考虑明显优于普通原型：不经 shell 的 `execve`、固定环境、进程组、超时、输出上限、崩溃和 malformed output 测试均有实现。缺点集中在协议解析、参数完整性、重走边和跨步状态。它更像一个可用的安全 adapter runner，而不是所有组合语义都已闭合的组合测试系统。

### 测试完备性

复核结果：

- Debug 构建的 CTest：19/19 通过。
- ASan/UBSan/LSan 构建的 CTest：19/19 通过。
- 测试覆盖模型、解析、生成确定性/限制、运行器 mismatch/malformed/timeout/crash/output cap、参数关系和 CLI。

但现有测试缺少：缺失参数、非自环重走、不同 seed 的差异、恶意/复杂 JSON、报告写入失败和跨步骤持久状态。绿色结果因此不能证明隐藏模型的完整组合正确性。

### AI 管理和维护

`.omo/plans`、draft、task evidence、final evidence、README 中英文同步约定都很完整，能够追踪计划、验证和安全边界。问题是 draft 状态仍有 awaiting/pending，而代码已完成，部分历史 evidence 仍停留在 18/19 测试，维护者需要区分当前状态与历史审计记录。

**评分：**

| 维度 | 分数 |
|---|---:|
| 目标匹配 | 8.5 |
| 灵活全面 | 7.0 |
| 可靠性 | 7.0 |
| 测试完备 | 7.5 |
| AI 管理可维护 | 8.5 |
| 综合 | **7.7** |

## 2. OMX 方案

### 实现与目标匹配

这是四个方案中验证闭环最强的一个。C11 API、文本 DSL、状态模式和 relation 模式均有清晰契约；trace 带版本、模型/IR/元数据 digest，replay 会校验输入一致性；状态场景支持子进程隔离、超时、快照/恢复和失败回滚；关系场景支持隐式 producer-consumer 依赖。

其目标与实际实现基本一致，尤其适合强调确定性、审计和可复现的测试基础设施。

### 灵活性、全面性和可靠性

优点：

- 状态图、调用关系、类型合同、结果合同、trace/replay、隔离、超时、快照恢复组成完整接口。
- 明确的 schema/version 兼容边界，未知必需指令不会静默改变行为。
- 具有 fuzz、sanitizer、valgrind、coverage、测量和 tampered replay 证据。
- 失败 scenario 的状态不会错误提交，适合有状态对象。

需要注意的限制：

- DSL 与 C API 的表达能力仍受 callback 设计约束，复杂业务状态需要用户实现 snapshot/restore。
- 关系调用的任意副作用不会提交到父进程，这是文档明确规定的边界，但使用者必须理解。
- 评审目录中的证据量很大，维护成本高于其他方案；需要稳定的 release-readiness 索引才能保持可读性。

### 测试完备性

复核 `make test` 通过：

- CLI smoke tests passed；
- API contract tests PASS；
- DSL boundary fuzz tests passed。

仓库还有多轮 evidence：coverage、sanitize、valgrind、trace/replay、tampered replay、性能测量和 SHA256 manifest。相较其他方案，它最接近把“测试工具本身也要被验证”落实为持续证据。仍应补充统一隐藏模型，尤其是复杂 JSON/参数错误、重复调用和真实多级状态对象。

### AI 管理和维护

`.omx` 下有 deep interview、PRD、architect/critic 多轮计划、ultragoal brief、迭代 evidence、release-readiness 和团队提交卫生报告。需求、设计、风险、验证和交付状态之间的链接最完整。缺点是记录数量多、部分 evidence 分散在迭代目录，需要一份当前实现与未完成项的单页索引。

**评分：**

| 维度 | 分数 |
|---|---:|
| 目标匹配 | 9.0 |
| 灵活全面 | 8.5 |
| 可靠性 | 8.5 |
| 测试完备 | 9.0 |
| AI 管理可维护 | 9.0 |
| 综合 | **8.8** |

## 3. Manual 方案

### 实现与目标匹配

功能面很广：状态机和函数模型两类 DSL，DFS/BFS/random/tour、多种覆盖统计、负向序列、资源绑定、guard、harness、动态库和 adapter。对于探索性组合测试，它的表达能力和用户可见选项最多。

但多个命令行契约与实际行为不一致：

- README 宣称 `--algorithm dfs`，显式传入却报 unknown algorithm；默认才走 DFS。
- BFS 忽略 `--max-cases`，tour 忽略 `--max-length`。
- 路径生成阶段不求 guard，可能生成不可执行路径。
- 嵌套状态普通执行可以识别父状态转换，但生成的 harness 只做叶状态精确匹配；实测 nested 模型的 `power_off` 在 `ON_IDLE` 下失败。
- 资源/值/class 使用 map，重复声明可能覆盖，重复定义检查很弱。

### 灵活性、全面性和可靠性

DSL 和算法覆盖面是优势，但“选项存在”不等于“语义完整”。guard、层级状态、并发、harness、direct execution 之间存在能力不一致；错误输入和边界行为较多依赖脚本 grep。未知 guard 变量/坏语法还可能静默当作 true，属于高风险默认行为。

### 测试完备性

`make test` 通过，但实际只有 `bash test/run.sh` 的 PASS。独立审查发现：

- 许多测试只 grep 输出，不检查退出码或 JSON 结构。
- harness-json 的失败断言以 `|| true` 吞掉。
- 缺少算法选项契约、max-cases/max-length、生效 guard、嵌套父状态、并发 harness、重复声明和失败 oracle 的系统测试。

因此测试数量或功能选项数不能代表可靠性。

### AI 管理和维护

`ai/proposal.md`、`design.md`、`task.md`、ADR、checklist、README 和 DSL 文档齐全，人工可读性较好。问题是同一 `task.md` 前部把 T24/T25/T28 写成完成，后部又列为未完成；设计/README 的能力宣称超前于实现。维护 bug 时容易依据错误状态作判断。

**评分：**

| 维度 | 分数 |
|---|---:|
| 目标匹配 | 7.0 |
| 灵活全面 | 7.5 |
| 可靠性 | 5.5 |
| 测试完备 | 5.0 |
| AI 管理可维护 | 8.0 |
| 综合 | **6.6** |

## 4. Plan/Goal 方案

### 实现与目标匹配

文档和任务治理非常完整，核心状态图、函数关系、adapter、harness、coverage、ASan 等能力均有设计。但实现存在明显目标偏差：

- `src/wise.cpp` 的函数流只枚举“所有函数各恰好一次”的完整拓扑排列，不生成函数子集、重复调用或空流程。对组合测试来说，这是隐藏问题 2 以及“缺失前置/重复调用”类问题的直接盲区。
- 状态生成中的 guard 剪枝语义不完整；某些情况下没有同时声明 expect 就直接放行。
- standalone 生成器把参数绑定写成注释，实际仍统一生成零参调用；main 的 standalone Runner 也没有传入 spec/guards/expect/outputs。
- direct 模式没有读取 `expected_outputs`，DSL 的输出期望只在 adapter 分支生效。
- DSL 标识符校验不足，恶意函数名可能注入生成的 C++。

### 灵活性、全面性和可靠性

设计上有状态 DSL、函数关系、约束、adapter、standalone、JSON 报告和 sanitizer，但 direct/standalone/adapter 三种路径的能力不一致。它的工程结构适合继续补齐语义，当前版本却不能把“描述了参数关系”解释为“standalone 真正传递了参数”。

### 测试完备性

复核 `make check` 返回 0，单元测试通过；`make asan` 也通过。可是示例 standalone 输出 14 条 flow 中仅 1 条 passed、13 条 failed，Makefile 以 `-` 忽略该命令失败，因此验收不会因预置失败路径失败。测试覆盖 parser/model/adapter/runner，但缺：

- 全组合语义 oracle；
- 函数子集和重复调用；
- standalone 参数实际传值；
- direct expect_output；
- 多状态/并发对象；
- 失败退出码是否被构建脚本正确传播。

### AI 管理和维护

这是最强项之一：`ai/proposal.md`、`design.md`、`decisions.md`、task/checklist、coverage、memory、measurement 文档齐全，任务拆分和设计决策可追踪。问题是若干 T23-T29 被写成完成，但代码仍有 standalone/参数对齐缺口；checklist 更像模板，缺最终审核签名和完成证据。

**评分：**

| 维度 | 分数 |
|---|---:|
| 目标匹配 | 6.5 |
| 灵活全面 | 7.0 |
| 可靠性 | 6.0 |
| 测试完备 | 7.0 |
| AI 管理可维护 | 8.0 |
| 综合 | **6.9** |

## 横向比较与排名

| 排名 | 方案 | 综合 | 最强点 | 首要风险 |
|---:|---|---:|---|---|
| 1 | OMX | **8.8** | 需求—设计—实现—验证闭环，trace/replay、隔离和证据最完整 | 证据目录复杂，需要维护当前状态索引 |
| 2 | LazyCodex | **7.7** | 工程分层清楚，adapter 安全边界和 19 项 CTest/ASan 很扎实 | 参数完整性、JSON 解析、重走边、跨步状态 |
| 3 | Plan/Goal | **6.9** | AI 设计和任务记录完整，文档治理强 | 核心组合语义只做全排列，standalone/direct 能力未对齐 |
| 4 | Manual | **6.6** | DSL 和选项最多，探索面最广 | 选项契约、guard/层级状态/harness 一致性和测试 oracle 不可靠 |

### 结论

如果目标是交付一个可审计、可复现、长期维护的组合测试基础设施，优先选择 **OMX 方案**。它不是因为“使用了 OMX”就自动领先，而是因为实际仓库中确实留下了完整的需求分解、架构审查、迭代证据、replay 篡改检测和 sanitizer/coverage/性能验证。

**LazyCodex** 适合作为第二选择，尤其适合需要安全 adapter 执行和清晰 CLI 的项目；补齐缺失参数校验、标准 JSON 解析、transition 重走和报告错误处理后，接近生产可用。

**Plan/Goal** 的文档治理值得保留，但在继续扩展前应先修正“全函数恰好一次”的生成语义，并让 direct、adapter、standalone 共享同一套参数、guard、expectation 执行路径。

**Manual** 适合快速探索和验证 DSL 想法，不宜直接作为高可靠版本。应先统一三种执行/生成模式的语义，修复命令行选项失效和嵌套状态 harness，再重写测试脚本，使失败退出码和 JSON 断言真正生效。

## 建议的共同后续验证

四个方案都应添加同一份跨实现回归模型，并把以下结果作为发布门槛：

1. 缺少必需参数必须拒绝；
2. 非自环状态边可按模型允许重复经过；
3. producer 返回值必须到达 consumer；
4. guard 不满足时不得生成“合法”路径；
5. 复杂但合法的 JSON adapter 响应必须正确解析；
6. 未知状态、超时、崩溃、SIGPIPE、报告写入失败必须得到区分明确的错误码；
7. standalone/harness 必须实际生成并传递参数；
8. 构建脚本不得用 `-` 或 `|| true` 吞掉产品失败；
9. 每个方案都应同时保存“需求/设计/任务状态/验证结果”的当前快照，避免文档把未完成工作标成完成。

