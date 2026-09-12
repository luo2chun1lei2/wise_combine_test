# OMX 独立复评：wise_combine_test

评审依据：`评价标准-组合测试项目.md`；评审对象为 `wise_combine_test.omx` 当前 HEAD `39850893cce6f0223296eadd791098aa7f7a6abe`。统一实验资产位于本目录及 `../common/`。本报告只记录独立复评证据，不修改被评项目源代码。

## 结论

该方案已经实现了可运行的 C11/Linux 状态图与函数关系组合测试器，具备版本化 DSL/C API/CLI、依赖排序、结果绑定、状态事务、进程隔离、超时、trace/replay 和 sanitizer/Valgrind 证据。统一 Queue oracle 的 24 次运行中，12 次干净对照全部通过，12 次对应 mutant 全部失败，6 个缺陷均被发现，发现率 100%，正常用例误报率 0%。

发布结论为 **有条件暂缓**：产品主路径达到测试阶段出口，但严格输入边界仍有 High 风险，且项目资料自身仍保留“待补回归项”与最终 I15 PASS 的矛盾。修复严格数值解析、重复顶层声明处理并重新生成边界证据后，才建议进入发布阶段。

## 统一 oracle 实验

适配器只把 Queue 原始返回值转成组合测试器的 callback result，断言全部写入模型 `contract ... expect=...`，没有在 SUT 或适配器中判断预期。Q3 的第二次 `q_open` 用模型别名 `q_reopen`，Q5 的两次 `q_pop` 用 `q_pop1/q_pop2`，这是由于该 DSL 要求 call ID 唯一而产生的明确适配成本。

| mutant | 缺陷 | 对应 case | 两次结果 |
|---:|---|---|---|
| 0 | clean | Q1–Q6 | 12/12 PASS |
| 1 | 空 pop 返回错误值 | Q1 | 2/2 FAIL |
| 2 | LIFO 代替 FIFO | Q2 | 2/2 FAIL |
| 3 | reopen 保留旧元素 | Q3 | 2/2 FAIL |
| 4 | peek 消费元素 | Q4 | 2/2 FAIL |
| 5 | 第二次 pop 不减少计数 | Q5 | 2/2 FAIL |
| 6 | close 后仍可 push | Q6 | 2/2 FAIL |

发现率：6/6 = 100%；干净用例通过率：12/12 = 100%；误报率：0/12。每个失败均返回 `call result assertion failed`，报告步骤和未完成数量，未出现崩溃或超时。

独立生成探针：非自环 cycle 被拒绝（`relation cycle`）；subset 模型成功；重复调用通过唯一别名表达。该实验是供应触发序列的执行验证，不证明工具能自动发现未声明的业务缺陷，也不证明穷举组合空间。

## 四层证据

### 1. 产品和交付物

- 状态图支持初始状态、迁移、可达边覆盖、分支前缀重放和事务 snapshot/restore；函数关系支持 DAG、显式 relation、`$producer` 结果依赖、arity/type/result contract、seed 和 bounded flows。
- `make clean && make test`、串行 `make clean && make sanitize`、`make clean && make coverage`、`make clean && make valgrind` 均通过；Valgrind ERROR SUMMARY 为 0。coverage 当前为 lines 18.87%、branches 23.58%、calls 23.36%，因此测试通过不能解释为全面覆盖。
- trace/replay 及 tampered trace 证据存在；统一 Queue 实验记录在 `queue-results.tsv`。

### 2. 需求、设计、测试资料

`.omx/interviews/`、`.omx/specs/`、`.omx/plans/`、`docs/api-contract.md`、`README.md` 和 `docs/release-readiness.md` 形成较完整的需求—设计—测试链，明确 C API/DSL/CLI、非目标和迭代门禁。设计对状态事务、关系 DAG、隔离和 trace 契约有具体说明。

资料缺口是 `docs/release-readiness.md:38-49` 仍把 null graph/report、allocation failure、若干 malformed/duplicate 场景列为“Before I1-I3 add”的待办，而 I15 `summary.json` 又宣称 PASS；应把未完成项、已验证项和发布范围重新对齐。

### 3. 项目管理资料

`.omx/ultragoal/goals.json`、`ledger.jsonl`、各轮 evidence manifest 记录了迭代、阻塞、提交、推送、测量和历史复盘；I15 manifest 绑定 HEAD、上一轮摘要哈希、命令和产物哈希。该层可追溯性较强，但早期 blocked 轮次和最终 PASS 的关系需要在 release checklist 中明确“历史记录不代表当前验证”。

### 4. 流程治理

项目有阶段门、每轮证据、sanitizer/Valgrind/replay/measurement 和 git push 规则，也记录过独立 code/architecture review。当前治理阻断点是新增的严格边界反例没有回写到产品测试和 release manifest，导致现有流程允许 I15 PASS 与实际 parser 边界缺陷并存。

## 发现的问题

### HIGH — 严格数值解析接受尾随字符

位置：`src/wct.c:242`、`src/wct.c:248`，CLI trace 解析在 `tools/wct_cli.c:227-238`。

`sscanf` 只要求读出前缀数字，没有确认完整 token。实测 `schema 1x` 被接受并执行（退出 0），`contract a 0x` 也被接受；将 trace 的 `seed 0` 改成 `seed 0x` 后 replay 仍为 PASS。该行为违反专项标准“非法值/版本/trace 数值必须拒绝”，可把损坏或篡改的模型/trace 当成合法输入，影响诊断复现和安全边界。应使用 `strtoul/strtoull` 并检查 `end`、范围和完整 token，补充模型及 replay 负例回归。

### MEDIUM — 重复顶层声明静默覆盖

位置：`src/wct.c:243`、`src/wct.c:246`。

重复 `state_graph` 或 `relation_graph` 指令会覆盖此前 ID/initial，而不是拒绝重复定义。实测两个 `state_graph` 和两个 `relation_graph` 模型均退出 0。专项标准要求重复定义可诊断拒绝；应在 parser 中记录声明是否已出现并报行号错误。

### MEDIUM — 测试覆盖率不足以支撑“测试全面”

现有 coverage 仅覆盖 18.87% 行、23.58% 分支。README 将 80% 写成可选目标，所以这不是需求硬失败，但对复杂的隔离、报告、错误和 parser 边界而言证据明显不足，应把关键分支矩阵列为发布必测项，或明确未覆盖风险。

## 七维评分（0–10）

| 维度 | 分数 | 依据 |
|---|---:|---|
| 模型表达和契约 | 9 | 状态、关系、参数、类型、结果和版本 DSL/C API 完整；无 guard/mutex/parallel/count/value 等能力，且非当前需求实现范围 |
| 组合生成正确性和覆盖 | 8 | 可达边、DAG、seed、bounded flows 和统一 oracle 均有效；不做未声明参数空间探索 |
| 执行可靠性、隔离与安全 | 8 | fork、超时、事务回滚、ASan/UBSan/Valgrind 证据充分；严格 parser 边界仍有 High |
| 报告、诊断和复现 | 8 | step/scenario/expected/actual、trace digest、tamper rejection 完整；尾随数字会削弱 replay 可信度 |
| 工具自身测试完备性 | 8 | API/CLI/fuzz/sanitizer/replay 和 24-run oracle 全通过；覆盖率低且仍有待办矩阵 |
| 设计、文档和后续维护 | 9 | 需求、设计、API、release、OMX 迭代资料完整，适配成本和非目标有记录 |
| 项目计划、风险和流程治理 | 9 | 迭代门、manifest、历史 blocker、commit/push、测量和复盘可追踪；PASS 与待办冲突需治理修正 |

平均分：8.4/10。由于存在 High 输入边界问题，按标准不得直接批准发布。

## 阶段门和补救

- 需求/设计：通过。需求编号、非目标、API/DSL、设计边界和 oracle 足够进入编码维护。
- 编码：有条件通过。核心实现和统一实验通过，但严格 token 解析与重复顶层声明必须修复。
- 测试：有条件通过。功能、内存和 replay 通过；加入上述四类 parser/replay 负例并重跑统一 oracle 后再通过。
- 发布：暂缓。修复 High、补齐回归证据、更新 I15 manifest/summary 和 release checklist，之后重新评审。
- 维护：通过但需跟踪。今后 DSL/API 变更必须同时更新需求矩阵、设计契约、fixture、边界回归和 evidence digest。

## 后续经验

1. `sscanf` 的前缀解析不适合作为版本化 DSL 和 trace 的合法性判断；输入契约应在设计阶段冻结“完整 token + 范围”规则。
2. 阶段门状态应由自动化检查从最新 evidence 生成，避免历史待办与最终 PASS 并存。
3. 统一 oracle 能验证执行器是否正确判定已给序列，但必须把“生成能力”和“触发序列由适配器提供”分开报告。
4. DSL 唯一 ID 约束提高可追溯性，同时会增加重复调用适配成本；应在用户文档和专项评分中显式记录。
