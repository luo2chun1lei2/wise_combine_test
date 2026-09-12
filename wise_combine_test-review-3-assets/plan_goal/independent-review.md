# Plan/Goal 方案独立复评

评审对象：`wise_combine_test.plan_goal`，评审标准版本：`评价标准-组合测试项目.md`。
本报告只写入 `wise_combine_test-review-2-assets/plan_goal/`，不修改被评项目源代码。

## 结论

阶段门结论：**有条件通过测试阶段，暂缓发布/下一阶段**。核心 parser、model、generator、adapter runner 和单元测试可运行；统一 Q1–Q6 对照实验中，12 个 clean run 均通过、12 个 mutant run 均出现真实失败，缺陷发现率 6/6（100%），clean 误报 0/12（0%）。这证明“给定模型和给定触发流程时，adapter 可以判错”，不证明自动发现能力或穷举组合覆盖。

不能直接发布的原因是三个高风险契约缺口：主程序即使流程失败也返回 0；direct/standalone 仍以无参 `int (*)()` 调用函数，参数关系只在 adapter 中真正传递；adapter 的字符串搜索协议既拒绝合法 JSON 变体，也接受未校验版本/尾随内容。因此产品的三种执行路径不具备统一语义，CI 和调用方可能把失败当成功，带参被测函数在 direct/standalone 中不能按需求执行。

## 1. 产品层证据

### 1.1 模型表达与生成

- DSL 能表达 object/state/transition、function、order、parameter、mutex、parallel、count/state/value 约束及有限 guard/expect 语法（`src/wise.cpp:423-803`）。
- 模型校验覆盖重复函数/对象、初始状态、未知引用、参数源、类型名、关系环和约束引用（`src/wise.cpp:805-1085`）。
- 状态路径有界 DFS，函数路径使用拓扑枚举；`--max-depth`、`--max-flows` 对各生成器生效（`src/wise.cpp:1108-1245`）。
- 独立 probe 发现 `--max-flows 1` 最终仍输出 2 条流程：state generator 和 function generator 各自使用上限，主程序在 `src/main.cpp:215-219` 直接拼接。这是预算和可观测性缺陷。
- 非 self cycle probe 生成了多个有界重复路径，说明有界环可以表达；但生成器同时会生成不带状态初始约束的完整函数排列，需在文档中明确这是关系组合而非状态可达路径。
- 缺少必需参数绑定的 probe `function consume(token t)` 被模型接受并生成 `consume`，未在 model 阶段拒绝；当前校验只检查已声明的 parameter relation，不检查每个函数参数是否有来源。
- `seed`、随机策略、replay 输入和稳定 flow ID 未实现；CLI 无 seed 选项（`src/main.cpp:85-159`），flow ID 仅是函数名连接（`src/wise.cpp:1924-1933`）。

### 1.2 执行与协议

- adapter 路径可按步骤启动外部进程、传递返回值、维护外部 state file，并正确识别 `ok`、mismatch、malformed、error 和 timeout（`src/wise.cpp:1593-1827`）。这也是本次统一实验实际使用的路径。
- direct 路径在 fork 子进程中用 `dlsym` 将所有函数强制解释为 `int (*)()`（`src/wise.cpp:1503-1516`）；standalone 生成器同样声明 `extern "C" int name()` 并调用 `name()`，参数绑定只写注释（`src/wise.cpp:1838-1862`）。因此设计中声明的 producer/consumer 参数不可能在这两条路径真实传递。
- adapter JSON 解析是字符串搜索而非完整 JSON 解析：要求精确的 `"key":"value"` 和 `"returns":{`（`src/wise.cpp:187-306`），不接受合法 JSON 的空格/字段顺序，也不完整处理转义；统一 probe 的合法空格响应被报告为 `malformed adapter response`。
- adapter 超时只 `kill(pid, SIGTERM)` 后等待（`src/wise.cpp:1706-1751`），没有进程组隔离、SIGKILL 兜底或后代清理；恶意/失控 adapter 可能遗留子孙进程。
- direct 有超时 alarm，但被测函数的 stdout 不捕获，`expect_output` 只在 adapter 中比对；任务文档明确 T31 仍待完成（`ai/task.md:223-228`）。

### 1.3 报告与退出状态

- 文本/JSON 报告包含 flow、status、exit_code、detail、bindings（`src/wise.cpp:1935-2013`），adapter 失败原因可定位到步骤类别。
- dry-run 的 `not_executed` 被故意计为非失败，且命令仍返回 0；这是可接受的预览语义，但应在报告中标记“未验证”。
- **High**：主程序无论 `results` 是否存在失败、timeout 或截断都在 `src/main.cpp:269-279` 返回 0。统一 adapter 的 12 个 mutant runs 虽报告 failed，shell 仍收到 exit 0；自动化门禁无法依赖退出码。
- **Medium**：flow ID 缺少模型文件、seed、配置、版本和重复步骤索引，无法满足专项标准的可复现审计要求。

## 2. 统一 Q1–Q6 实验

资产：`../common/cases.json`、`queue_sut.h`、`README.md`；模型和 adapter 见本目录。每个模型使用状态路径，并为重复/别名函数建立模型名（如 `q_pop_empty`、`q_pop_first`）；adapter 只把原始返回值转为协议并用私有 state file 保存 QueueState，没有在 adapter 中作断言。每个 case 运行 clean 与对应 mutant 各两次，共 24 runs。

| run | case | variant | repeat | tool exit | passed/failed | oracle verdict |
|---:|:---:|:---:|---:|---:|---:|:---:|
| 1 | Q1 | clean | 1 | 0 | 2/0 | PASS |
| 2 | Q1 | clean | 2 | 0 | 2/0 | PASS |
| 3 | Q1 | mutant | 1 | 0 | 0/2 | PASS |
| 4 | Q1 | mutant | 2 | 0 | 0/2 | PASS |
| 5 | Q2 | clean | 1 | 0 | 2/0 | PASS |
| 6 | Q2 | clean | 2 | 0 | 2/0 | PASS |
| 7 | Q2 | mutant | 1 | 0 | 0/2 | PASS |
| 8 | Q2 | mutant | 2 | 0 | 0/2 | PASS |
| 9 | Q3 | clean | 1 | 0 | 2/0 | PASS |
| 10 | Q3 | clean | 2 | 0 | 2/0 | PASS |
| 11 | Q3 | mutant | 1 | 0 | 0/2 | PASS |
| 12 | Q3 | mutant | 2 | 0 | 0/2 | PASS |
| 13 | Q4 | clean | 1 | 0 | 2/0 | PASS |
| 14 | Q4 | clean | 2 | 0 | 2/0 | PASS |
| 15 | Q4 | mutant | 1 | 0 | 0/2 | PASS |
| 16 | Q4 | mutant | 2 | 0 | 0/2 | PASS |
| 17 | Q5 | clean | 1 | 0 | 2/0 | PASS |
| 18 | Q5 | clean | 2 | 0 | 2/0 | PASS |
| 19 | Q5 | mutant | 1 | 0 | 0/2 | PASS |
| 20 | Q5 | mutant | 2 | 0 | 0/2 | PASS |
| 21 | Q6 | clean | 1 | 0 | 2/0 | PASS |
| 22 | Q6 | clean | 2 | 0 | 2/0 | PASS |
| 23 | Q6 | mutant | 1 | 0 | 0/2 | PASS |
| 24 | Q6 | mutant | 2 | 0 | 0/2 | PASS |

统计：clean 12/12 通过、误报 0；mutant 12/12 出现失败；按缺陷种类 Q1–Q6，发现率 6/6 = 100%。每个 case 实际生成 2 条状态/函数流程，mutant 失败来自状态路径和/或函数路径的预期返回检查。`tool exit` 全部为 0，进一步证明退出码缺陷不是理论问题。该实验不计作自动发现证明：模型已把触发序列写入 transition，且别名映射由评审适配器承担，重复调用覆盖仍需产品原生 DSL 证据。

## 3. 四层评审

### 3.1 产品和交付物

标签覆盖 `[CT-表达能力] [CT-生成正确性] [CT-执行可靠性] [CT-诊断复现] [CT-测试完备性] [CT-扩展维护] [CT-可观测性]`。

- 正面：核心模块可编译；`test/test_wise.cpp` 839 行，单元测试通过；adapter 异常 fixture、guard、参数绑定、约束和日志轮转均有测试；覆盖报告声明行 80.17%、分支 81.47%。
- 缺口：没有统一模型 × 生成策略 × 执行模式 × 故障类型矩阵；没有 seed/replay/稳定 ID；direct/standalone 不支持真实参数/输出；协议解析不符合 JSON 语法；报告失败写入只静默忽略（`src/wise.cpp:1914-1921`）。
- `make check` 表面返回 0，但 Makefile 第 17 行使用 `-./build/wise_standalone` 忽略独立程序失败；实际 standalone 输出 14 条流程，其中 13 条 failed。这使验收证据不可信。

### 3.2 需求、设计、测试文档

正面：`ai/proposal.md`、`ai/design.md`、`ai/task.md`、`ai/decisions.md` 与三份 checklist 存在，决策 D1–D11 可追溯任务号；DSL、架构、执行模式、日志策略均有说明。

缺口：需求把参数传递列为核心能力，但设计的 direct/standalone 固定无参 ABI 与此冲突；T30/T31 仍待以后完成，却没有把 direct/standalone 的不支持边界提升为发布前阻断项；测试设计没有把完整 JSON 语法、进程后代清理、退出码、replay、跨模式一致性列为必测。文档中的“类型”与实现解析的返回名也缺少清晰契约。

### 3.3 项目管理资料

任务拆分覆盖 parser、model、generator、runner、report、测试、ASan、测量和文档，并有依赖图。风险部分覆盖组合爆炸、状态空间、动态代码和依赖限制。

缺口：T30/T31 被标成可选/待以后，但它们直接影响已声明能力，优先级和阶段门未更新；没有明确缺陷严重度、负责人、截止日期、回归证据；资源测量只做小规模 max-flows，未量化 adapter 进程数、超时、日志写入失败和状态文件成本；git remote 存在，但本次复评不把远程推送历史视为产品质量证据。

### 3.4 流程治理和变更管控

AGENTS.md 要求 proposal → design → task，且每次 ai 文档变更后有 checklist；提交历史显示增强任务和决策有独立提交，具备基本变更追踪。

缺口：没有自动阶段门检查，`make check` 忽略失败；没有统一 oracle 资产、缺陷回归清单、报告版本或模型兼容性矩阵；跨模式变更（adapter 增强）没有强制 direct/standalone 对齐；日志/报告写入错误不升级为流程失败。后续每个 DSL 或执行协议变更都应强制更新 proposal/design/task、fixture、24-run 对照或等价回归，以及兼容性说明。

## 4. 严重问题与补救

### HIGH

1. **执行失败被返回码吞掉** — `src/main.cpp:269-279`。影响 CI、发布门和所有脚本自动判断。修复：存在 failed/crashed/timeout，或生成被截断时返回非零；dry-run 明确单独退出语义；补回归测试。
2. **direct/standalone 丢失参数语义** — `src/wise.cpp:1503-1516,1838-1862`。影响核心 producer-consumer 需求和跨模式一致性。修复：引入类型化调用契约/生成真实参数传递，或明确把带参执行限制为 adapter 并从核心承诺中删除；三模式对同一 fixture 做一致性回归。
3. **adapter 不是完整 JSON 解析器** — `src/wise.cpp:187-306,1763-1775`。影响合法协议输入、字段顺序/空格/转义和未知字段诊断。修复：实现严格、可见失败的 JSON parser 或冻结并验证明确的 canonical wire format；覆盖 10 类协议负例。
4. **必需函数参数可在没有来源时继续生成/执行** — `src/wise.cpp:820-927,1268-1290,1607-1632`。`parameter_respected` 只验证已经声明的关系，未要求每个参数都有 relation/常量；adapter 随后发送空 args。修复：在模型校验阶段为每个必需参数验证唯一来源或明确的运行时输入，并补缺失、重复、类型错误回归。

### MEDIUM

5. **max-flows 不是全局上限** — `src/main.cpp:215-219`；分别生成后拼接导致超预算。修复：统一计数器/预算分配并在报告中列出截断来源。
6. **超时不清理后代进程** — `src/wise.cpp:1706-1751`。修复：进程组、超时后 SIGTERM→SIGKILL、wait/清理和后代测试。
7. **诊断缺少 seed/config/version/replay 且 flow ID 可碰撞** — `src/wise.cpp:1924-1933`。修复：结构化稳定 ID、模型摘要、seed、环境和可重放输入。
8. **报告/日志写入失败静默忽略** — `src/wise.cpp:1914-1921`。修复：暴露 I/O 错误并让阶段门失败。
9. **执行模式输出语义不一致** — direct 不支持 expect_output，standalone 只生成注释；T31 未完成。修复：实现统一 stdout 捕获，或把限制写入发布契约并在 CLI 中拒绝不支持组合。

## 5. 七维评分

评分采用专项标准 0–10 锚点；证据不足和 High 硬门槛会限制推荐，不用平均分掩盖阻断项。

| 维度 | 分数 | 依据 |
|---|---:|---|
| 模型表达和契约 | 7.0 | DSL/校验较广，但无类型化 adapter 标量、重复调用别名、版本兼容和完整 oracle 契约 |
| 组合生成正确性和覆盖 | 6.5 | 有界 DFS/拓扑/约束可运行；max-flows 拼接超限、seed/replay/覆盖分母缺失 |
| 执行可靠性、隔离与安全 | 5.0 | adapter 隔离和 timeout 基本可用；direct/standalone 丢参，后代清理和 ABI 边界不足 |
| 报告、诊断和复现 | 5.5 | JSON/text 有失败详情和 bindings；退出码恒 0、ID/seed/config/replay 缺失、I/O 静默 |
| 工具自身测试完备性 | 7.0 | 单测/ASan/fixture/覆盖率较好，统一实验 6/6；缺少完整协议、后代、跨模式和阶段门回归 |
| 设计、文档和后续维护 | 7.0 | proposal/design/task/decisions/checklists 齐；核心承诺与实现冲突，T30/T31 发布边界未闭合 |
| 项目计划、风险和流程治理 | 6.0 | 任务依赖、风险和提交历史存在；没有阻断性阶段门、owner/deadline、统一回归资产和失败升级 |
| **平均** | **6.3** | High 问题存在，不能据平均分发布或排名第一 |

## 6. 下一阶段与经验

进入下一阶段前必须完成：退出码和 Make gate 修复；决定并实现或正式降级 direct/standalone 参数契约；替换/冻结严格协议并补完整 JSON 测试；统一 max-flows；补进程组清理、稳定 replay 元数据和日志失败测试。完成后重跑 Q1–Q6 24-run 对照，增加 direct/standalone 可映射 fixture；所有失败结果须在 shell 层非零退出。

可复用经验：组合测试评审必须把“生成、执行、判定、报告、退出码”视为同一契约；仅有单元测试和覆盖率不能证明跨模式一致；阶段门必须执行失败即失败；统一 mutant 对照应保存模型、命令、adapter、输出和环境，而不是只引用 README。
