# 组合测试四个方案：架构复核

## 结论

本复核只检查“模型 → 生成 → 执行 → 报告”的边界、状态和参数流，不替代四个独立执行评审。共同 Q1–Q6 的结果显示四个适配方案都能在**预先给定触发序列**时把对应 mutant 判为失败；这证明了适配后的执行断言链，不证明各工具能自动生成这些序列，也不证明组合空间穷尽。共同实验明确了这一边界（`wise_combine_test-review-2-assets/common/README.md:1-17`）。

架构状态按“需求基线是否冻结该能力”判定，而不是把所有扩展探针当成缺陷：OMX 为 `WATCH`；LazyCodex、Manual、Plan/Goal 的 `BLOCK*` 只在其公开/专项基线把相应契约列为必需时成立（星号表示需核对需求冻结）。如果能力被明确标为 Optional/Unsupported，则降为 `WATCH` 或记录为能力边界。这不否定它们对 Q1–Q6 的相对结果，而是说明不能把“测试脚本通过”直接解释成通用组合测试工具可靠。

### 严重性校准

- `BLOCK` 仅用于已宣称或专项必需的契约被静默接受、跨执行模式改变语义、或测试/报告无法可信归因；例如必填参数漏检、已公开的参数流在 direct/standalone 中丢失。
- 空流程、子集、重复调用、负向场景和高级约束先按 `Required / Advertised / Optional / Unsupported` 标注。没有冻结为 Required/Advertised 时，缺失只能记为表达能力或覆盖范围的 `WATCH`，不能直接升级为产品阻断。
- 报告格式、覆盖率合并、日志目录失败通常是 `Medium/WATCH`；只有它们能绕过安全边界、吞掉核心失败或使发布结论不可判定时才升级 `BLOCK`。

## 统一证据和公平性边界

### 共同实验实际测量了什么

- Q1–Q6 使用相同的有状态 C queue 和一次一个 mutant 的控制组；控制结果文件记录 clean 每次通过、匹配 mutant 每次产生真实 mismatch（`common/control-results.json`）。
- 四个适配器都把重复调用映射为不同模型 ID：OMX 的 `q_pop1/q_pop2`、`q_push1a/q_push1b` 在 `review-2-assets/omx/queue_harness.c:18-28`；Manual 用 `q_pop_two/q_pop_one` 映射同一个 `q_pop` 符号（`review-2-assets/manual/q2.dsl:23-35`）；Plan/Goal 用 `q_pop_first/q_pop_second` 等别名并由 adapter 前缀派发（`review-2-assets/plan_goal/Q5.ct:9-26`、`plan_goal/adapter.cpp:10-14`）。这些是测试适配层的必要语义桥，不能计为原生 DSL 已经支持重复调用。
- 每步独立进程的方案通过私有状态文件保留 queue 状态：LazyCodex 的 adapter 明确 restore/save（`review-2-assets/lazycodex/queue_adapter.cpp:18-31`）；Plan/Goal 使用固定 `/tmp/wct_queue_state.bin`，可由参数覆盖（`review-2-assets/plan_goal/adapter.cpp:5-10`）。因此 Q3–Q5 的状态连续性依赖 adapter 桥，而不是四个 runner 天然共享一个被测对象。
- Manual 记录了 clean 场景被生成的多条序列中选择目标序列，随后生成 harness 并执行（`review-2-assets/manual/run_oracle.sh:25-31`）；这同样是 supplied-trigger 测试，不是自动发现率。

### 归因规则

将“未发现 mutant”归因给产品，仅当：模型能够表达序列、生成器产出或可明确报告无法产出、执行器执行了该序列、oracle 在工具之外冻结且 adapter 没有断言。若 adapter 修改了函数名、参数签名、进程状态或期望字段，结论必须拆成“产品能力”和“适配器可表达性”。

## LazyCodex

**状态：BLOCK（条件性）**。线性 JSON 状态图和 adapter 执行闭环能覆盖 Q1–Q6，但核心模型契约与生成策略没有完全闭合；若项目标准未要求 seed 可复现或多进程上限，相关项应降级为 `WATCH`。

### 模型 → 生成

- 函数参数只在 transition 提供的 `args` 中逐项检查类型；`Model::validate` 没有检查函数声明的必需参数是否全部出现（`wise_combine_test.lazycodex/src/model/model.cpp:130-141`）。因此“缺少必需参数”会穿过模型层，属于 `[CT-表达能力] [CT-生成正确性]` 的产品缺陷，而非测试适配问题。
- 生成器把非自环 transition 在同一 sequence 中出现过即排除（`wise_combine_test.lazycodex/src/generate/generate.cpp:27-40`）；非自环回路无法重走。`seed` 参数在入口被显式丢弃（`:76-79`），选择又按 ID 排序（`:43-45`），所以 seed/随机策略契约实际上没有接到生成器。
- `max_cases` 和 `max_steps` 接在递归边界（`:21-24,53-69`），但 `max_subprocesses` 只在模型校验中要求为正，运行时没有并发/资源调度实现（`src/model/model.cpp:117-123`、`src/runtime/runtime.cpp:153-173`）。这使配置表面上支持三种限制，实际只执行两种。

### 生成 → 执行/状态

- 运行器按 flow 的每一步 fork 一个 adapter 进程，并把 producer return 写回后续参数（`wise_combine_test.lazycodex/src/runtime/runtime.cpp:153-173`）；Q 实验需外部 state file 才能跨步。产品自身没有状态提交/回滚契约，adapter 文件失败也只会转成 launch/protocol 类失败。
- 协议解析是字符串查找和首个 `}` 截断（`:54-77,80-97`），不是完整 JSON parser；字段顺序、合法空白/转义、未知 status、嵌套返回对象会改变判定。报告写入失败也被忽略（`src/report/report.cpp:14`、`src/cli/cli.cpp:174-182`）。这是执行/报告边界的可靠性风险。

### 解释 Q1–Q6

`lazycodex/results.csv:2-25` 表明 24 次 clean/matching-mutant 运行均得到预期 pass/fail；因此它的 supplied-trigger 断言有效。不能据此推导 seed 可用、重复边可生成或缺参会被拒绝。

## OMX

**状态：WATCH。** 这是四个实现中边界最清楚的一个：状态 runner 要求 snapshot/restore，关系 runner 要求 DAG 并显式处理 `$producer` 结果，失败和覆盖计数有独立字段。

### 模型 → 生成

- 关系校验覆盖调用 ID、arity、参数类型、未知 `$call`、重复边和 DAG 环（`wise_combine_test.omx/src/wct.c:333-432`）；执行前再次校验，ready call 按 seed 或稳定 ID 选择（`:662-720`）。
- 参数传递是字符串 `$call` 结果替换，不是跨状态对象或类型化资源绑定（`:724-731`）。这符合当前 C API 的明确边界，但对“有状态对象 + 多个参数实例”的组合测试能力有限，应标记明确不支持，不能把它算成隐藏 bug。
- state runner 使用 `seen` 边集和 branch replay，要求 snapshot/restore，并在断言通过后提交 post-state（`:435-624`）；API 文档也把该契约写明（`docs/api-contract.md:19-43`）。

### 执行 → 报告

- 关系 runner 保存每个 call 的结果，失败时记录 scenario、expected、actual，覆盖以 call/edge 分开计算（`src/wct.c:733-794`）；整个 scenario 默认由 child 承担并有 deadline/kill-reap（`:797-830`）。
- Q 实验的 `queue-results.tsv:2-85` 记录 clean 14 次全通过、每个匹配 mutant 两次失败；`generation-probes.txt:1-6` 显示 cycle 拒绝、subset/repeat 可执行。这里的 repeat 仍由 distinct call IDs/adapter aliases 表达，不是同一 call 节点可自然重复。

### 覆盖率子进程风险

`Makefile:65-72` 只用 `gcov -o build src/wct.c` 汇总父工程的 `.gcda`；测试命令会 fork/child 执行隔离路径，但没有 `GCOV_PREFIX`/子进程合并策略、清单或按 child 归因规则。现有报告只有 18.87% 行、23.58% 分支（`coverage/wct.gcov.txt:1-5`）。这至少是 `WATCH`：数字可以作为本次构建的观测值，但不能被解释成所有隔离/回滚路径已覆盖，直到补充子进程覆盖合并或明确排除策略。

## Manual

**状态：BLOCK（条件性）**。该方案的 DSL/生成器表面能力最宽，包含资源状态、值域、负向序列、嵌套/历史/并发状态和多种 harness；这些能力若确属公开承诺，则阶段间语义不一致构成阻断；若仅为路线图/扩展，应分别记为 `WATCH`。

### 模型 → 生成

- 函数模型会把 resource、requires、effects、success、C signature 解析到内部模型并校验引用（`wise_combine_test.manual/src/function_model_builder.cpp:56-131,178-279`）；生成器按资源实例和状态要求绑定，值参数从 range/list 采样（`src/sequence_generator.cpp:288-341`）。这解释了 Q 模型为什么需要 `q_pop_two/q_pop_one` 等别名。
- 状态路径生成只按 active state 匹配 transition，DFS 在 `state_machine_path_generator.cpp:181-200` 没有求值 guard；guard 是执行时才判断。因此生成结果可能是不可执行路径，生成覆盖不能等同于可执行覆盖。

### 生成 → harness/执行

- 函数 harness 能按 DSL signature 传参数、保存资源句柄、检查 success 和 observe effects（`src/harness_generator.cpp:220-333`），这一边界是闭合的。
- 状态 harness 把当前值固定为 `leafOf(initial)`，transition 匹配也使用精确 `strcmp(current, transition.from)`（`src/harness_generator.cpp:410-447`）；而普通状态执行器允许父状态/后代匹配（`src/state_machine_path_generator.cpp:16-39`）。嵌套状态的“生成/执行”不是同一语义，属于架构级不一致。
- 共同实验脚本从所有生成序列中挑出目标序列并再生成 harness（`review-2-assets/manual/run_oracle.sh:26-31`），所以 Q1–Q6 只能证明函数 harness 的断言链；它没有验证复杂状态机生成/执行一致性。

### 测试结果与测试管控

`review-2-assets/manual/results.tsv:2-25` 记录 clean Q2–Q5 为 `FAIL`，matching mutants 为 `PASS`。这不是 queue 产品失败：脚本把 clean harness 的非零结果作为预期逻辑通过，字段名 `status` 容易误读，报告必须明确“PASS=达到预期 oracle”。此外，仓库测试脚本存在 `|| true` 吞失败风险（`wise_combine_test.manual/test/run.sh:89-92`），不能只依据 shell 退出码。

## Plan/Goal

**状态：BLOCK（条件性）**。该方案的 parser/model/runner 设计文档完整，adapter 模式确实传递参数和返回值；若 direct、adapter、standalone 都是公开执行模式，三条边界没有共享同一个参数/期望契约即为阻断；若 standalone 仅是实验性导出，则记录为 `WATCH`。

### 模型 → 生成

- Model 校验状态、函数、参数关系、order DAG、mutex、parallel、count/value constraint（`wise_combine_test.plan_goal/src/wise.cpp:820-1072`）。
- state DFS 只在 final state 输出 flow，并应用 parameter/order/parallel/state 条件，未应用 mutex/count/value（`:1133-1182`）；function generator 则只在所有函数各出现一次时输出拓扑排列（`:1184-1244`）。因此一个“全函数一次”的模型不能表达空流程、子集或重复调用；Q1–Q6 是通过别名和线性状态图绕过这一生成边界的。

### 生成 → 执行

- adapter runner 对 parameter relation 做真实 `returned` lookup，逐步建立 JSON request（`src/wise.cpp:1593-1632,1685-1822`），所以 Q 实验中返回值桥接是实际执行的。
- direct runner 对每个 flow 函数都用 `int (*)()`、无参数调用（`:1503-1517`）。standalone 生成器也只生成 `name()`；parameter relation 在生成源中只是注释（`:1844-1877`）。
- CLI standalone 分支没有把已构造的 `spec`、guards、expected returns/outputs 传给 Runner，只用 `{o.lib_path, true, 10, {}}`（`src/main.cpp:194-213,226-228`）。所以 README/设计声称的 standalone 与 adapter/direct 语义不是同一实现。

### 结果归因

`plan_goal/results/summary.tsv:2-25` 的 24 次记录均达到脚本预期，但 `run_experiment.sh:23-31` 只判断报告中的 passed/failed 数，并且每个 Q 模型把完整目标调用链写成线性状态流（例如 `Q5.ct:9-26`）。这足以证明 adapter 模式对六个已给定序列有效；不证明默认 function generator 支持子集/重复，也不证明 standalone 传参。

## 横向架构结论

| 方案 | 模型/生成边界 | 状态/参数边界 | 模式一致性 | 架构状态 |
|---|---|---|---|---|
| LazyCodex | JSON schema 清楚，但缺参漏检；seed 被忽略（若 seed 是必需契约）；非自环重复受限 | 每步进程依赖外部 state file；返回值可传但无事务状态 | 只有 adapter runner | **BLOCK（条件性）** |
| OMX | state/relation 两套契约均有验证和覆盖字段 | snapshot/restore、`$call` 结果和 child isolation 明确 | C API/CLI 同一 runner 语义较一致 | **WATCH** |
| Manual | DSL/资源/状态能力最广；状态生成不求 guard | 函数 harness 传参/观测较完整，状态 harness 叶状态语义不同 | direct/dylib/harness 多模式，状态模式有分叉 | **BLOCK（条件性）** |
| Plan/Goal | 校验丰富，但状态流少约束、函数流只全排列（扩展缺口，除非子集/重复为必需） | adapter 有参数流，direct/standalone 固定零参 | 三种模式契约不一致 | **BLOCK（条件性）** |

## 建议并入标准的架构检查项

1. 在专项标准的“执行器与适配层”中增加“同一模型至少在两种执行模式运行时，参数、返回值、状态提交/回滚、期望判定和失败分类必须逐字段对齐；仅支持一种模式应标记明确不支持”。
2. 在“统一隐藏问题 oracle”中把“触发序列由评审提供”与“生成器自动产出”拆成两个证据项；适配器别名、外部状态文件和 harness 生成均单独记录，不计为原生表达能力。
3. 在通用标准的证据规则中增加“fork/exec/生成子进程的覆盖率必须说明数据文件归属、合并方式和遗漏边界；代码覆盖率不能证明子进程路径已覆盖”。
4. 增加“清洁场景失败的期望语义”字段：负向返回是成功的负测试，runner/adapter/tool 崩溃是失败；脚本不能用 `|| true` 掩盖未判定状态。
5. 增加“阶段契约闭合”检查：模型字段 → IR → 生成约束 → 执行参数/状态 → 报告字段逐项列矩阵；任何只在注释、日志或报告出现而未进入执行的数据流必须判为未实现。

## References

- `wise_combine_test-review-2-assets/common/README.md:1-17` — 共同实验边界和预算。
- `wise_combine_test.lazycodex/src/generate/generate.cpp:21-101` — LazyCodex 生成边界。
- `wise_combine_test.lazycodex/src/runtime/runtime.cpp:54-173` — LazyCodex 协议、进程和返回值流。
- `wise_combine_test.omx/src/wct.c:333-432,435-624,662-830` — OMX 校验、状态/关系执行和隔离。
- `wise_combine_test.manual/src/state_machine_path_generator.cpp:181-200` — Manual 生成器未求 guard。
- `wise_combine_test.manual/src/harness_generator.cpp:410-447` — Manual 状态 harness 的精确叶状态匹配。
- `wise_combine_test.plan_goal/src/wise.cpp:1133-1244,1473-1591,1593-1828,1830-1890` — Plan/Goal 生成、direct、adapter、standalone 边界。
- `wise_combine_test.plan_goal/src/main.cpp:194-228` — Plan/Goal standalone 丢失 spec/期望配置。
