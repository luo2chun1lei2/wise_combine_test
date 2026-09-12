# wise_combine_test 四方案第二次评审

评审标准： [评价标准-组合测试项目.md](/home/workspace_data/works/myprojects/组合测试评价/评价标准-组合测试项目.md)  
通用标准版本：2.2（2026-09-12）  
评审对象版本、命令、日志和实验资产：[/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets)

本次复评把四个项目按相同的组合测试专项标准重新检查，并由四个独立 subagent 分别负责项目复评，另有独立架构复核。与第一次评审相比，重点增加了产品—工程资料—项目管理—流程治理四层证据、阶段门结论、能力边界分类和统一有状态 oracle。

## 1. 评审范围和判定口径

专项标准中的能力先按项目自身契约分类：

- **核心契约**：需求、README、公开 API 或设计明确承诺的能力；
- **已声明的可选扩展**：项目声称支持，但不属于本次核心交付；
- **明确非目标**：文档明确排除；
- **尚未冻结/尚未验证**：不能直接当成通过，也不能在没有需求依据时当成产品缺陷。

空流程、子集、重复非自环、负向生成、guard/mutex/parallel/count/value、随机策略和多对象交互只有在项目承诺或本次冻结需求中属于必需能力时，才作为核心阻断项。不同方案有不同 DSL，实验适配器只负责协议转换、原始结果传递和状态桥接；适配器不能替产品补写核心断言或自动生成缺失流程。

本次 Q1–Q6 实验使用相同的有状态队列 SUT 和 6 个单独 mutant。控制组已独立验证：12 次 clean 运行全部满足 oracle，12 次 mutant 运行全部产生预期差异。每个项目的结果还要区分：

1. 模型能否表达该流程；
2. 生成器能否自动产生该流程；
3. 执行器面对供应的流程能否正确判错；
4. 报告和退出码能否准确复现失败。

因此，供应触发流程的 6/6 发现率只证明执行判定链，不能证明组合生成器会自动探索到这 6 个流程。

## 2. 统一实验结果

| 方案 | clean 对照 | matching mutant | 缺陷种类发现率 | 正常误报率 | 主要限制 |
|---|---:|---:|---:|---:|---|
| OMX | 12/12 通过 | 12/12 判失败 | 6/6 = 100% | 0/12 | Q3/Q5 使用唯一 call ID 别名；外部状态桥 |
| LazyCodex | 12/12 通过 | 12/12 判失败 | 6/6 = 100% | 0/12 | 每步新进程，使用私有状态文件 |
| Plan/Goal | 12/12 通过 | 12/12 报告失败 | 6/6 = 100% | 0/12 | adapter 状态文件、别名；所有命令退出码为 0 |
| Manual | 未形成可接受的统一 24-run 结果 | 不计发现率 | 未验证 | 未验证 | 生成器先枚举大量额外序列，精确触发脚本超时/中断；不能用部分资产证明 6/6 |

Manual 不能因为无法完成统一适配就直接判工具失败；但它也不能获得其他方案已实际获得的 oracle 证据。其公开自带 `make test` 仍通过，另有独立静态和历史嵌套 harness 证据。

完整资产包括：[common oracle](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets/common/README.md)、[LazyCodex 复评](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets/lazycodex/independent-review.md)、[OMX 复评](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets/omx/independent-review.md)、[Manual 复评](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets/manual/independent-review.md)、[Plan/Goal 复评](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets/plan_goal/independent-review.md)、[架构复核](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets/architecture-review.md)。Manual 的适配资产和中断日志也保留在其 review-assets 目录中，并按“未验证”处理。

## 3. OMX 方案

### 产品和交付物

OMX 实现了 C11/Linux 状态图和函数关系组合测试器，具备版本化 DSL/C API/CLI、依赖排序、返回值关系、状态事务、snapshot/restore、进程隔离、超时、trace/replay 和 sanitizer/Valgrind 证据。Q1–Q6 的 clean 12/12、mutant 12/12 通过，说明给定流程的 callback、contract、失败报告链有效。

它的参数模型主要是字符串参数和前序 call 结果，不是通用上下文对象或多值资源绑定。项目文档将这一点作为边界时，不应误报为缺陷。

实际发现的问题：

- `src/wct.c:242,248` 和 `tools/wct_cli.c:227-238` 使用 `sscanf`，接受 `schema 1x`、`contract a 0x` 和被篡改的 `seed 0x`；这削弱 DSL/trace 完整性和 replay 可信度，列 Medium；
- 重复顶层 `state_graph`/`relation_graph` 声明会静默覆盖，未按专项标准拒绝；
- coverage 实测 lines 18.87%、branches 23.58%、calls 23.36%。由于项目将 80% 作为可选目标，这不是单独的发布阻断；但 fork/child 覆盖归集策略没有清楚证明，覆盖数字只能作为 WATCH 证据；
- `docs/release-readiness.md` 仍有待补的 null/report/allocation/malformed/duplicate 项，而 I15 summary 又标 PASS，阶段状态需要对齐。

### 工程、管理和流程资料

`.omx/interviews`、`.omx/specs`、`.omx/plans`、`.omx/ultragoal`、ledger、manifest、API contract 和 release-readiness 形成四层追踪链。计划、迭代、提交、测量和历史 blocker 可追踪；但当前 release checklist 未将新发现的 parser 边界纳入最终门禁。

### 七项评分

| 维度 | 分数 |
|---|---:|
| 模型表达和契约 | 9.0 |
| 组合生成正确性和覆盖 | 8.0 |
| 执行可靠性、隔离与安全 | 8.0 |
| 报告、诊断和复现 | 8.0 |
| 工具自身测试完备性 | 8.0 |
| 设计、文档和后续维护 | 9.0 |
| 项目计划、风险和流程治理 | 9.0 |
| **平均** | **8.4** |

### 阶段门

**需求/设计：通过。编码：有条件通过。测试：有条件通过。发布：暂缓。维护：有条件通过。**

发布前修复严格 token 解析、重复声明处理，补边界回归并更新 release manifest；coverage 需说明 child 数据归集策略。当前不存在因未支持上下文对象而产生的核心阻断，除非项目今后把它列为必需能力。

## 4. LazyCodex 方案

### 产品和交付物

LazyCodex 的分层实现、JSON 模型、状态/函数/argument/before 关系、adapter 进程组、固定环境、超时、输出上限和 19 项 CTest 较完整。Debug CTest 19/19、ASan CTest 19/19，Q1–Q6 clean/mutant 发现率 6/6，执行判定链有效。

关键问题：

- `src/model/model.cpp:130-141` 未校验函数声明的必需参数是否都有来源；核心参数契约若要求“缺失来源必须在模型阶段拒绝”，这是 BLOCK；
- `src/generate/generate.cpp:76-79` 丢弃 seed；若 seed 是公开配置而不是明确 deterministic-only 边界，这是 Medium；
- 非自环 transition 重走、guard/mutex/parallel/count/value/resource/负向流程和复杂返回值 oracle 没有表达或验证；应按项目范围分别标为未支持或能力缺口；
- adapter 使用字符串扫描解析 JSON，对完整合法 JSON 的空格、字段顺序、转义和嵌套鲁棒性未证明；
- `--reports` 指向普通文件时 `create_directories` 抛异常并以 134 终止，没有稳定的文档化错误；
- step report 缺少完整 args、expected、seed、模型/环境、trace/replay/hash，难以独立复现复杂失败；
- `.agents` 被忽略而 AGENTS 又要求 AI 文件追踪，且 PROJECT KB 仍写 requirements-only/no implementation，维护记录需清理。

### 工程、管理和流程资料

`.omo/plans`、`.omo/evidence`、README 中英文和分层源码形成较好的实现链；但缺独立 architecture/design 对比，需求没有把 seed、返回值 oracle、约束和 coverage 定义冻结为可验收契约。成本、AI 调用量、计划偏差和发布回滚记录不完整。

### 七项评分

| 维度 | 分数 |
|---|---:|
| 模型表达和契约 | 6.0 |
| 组合生成正确性和覆盖 | 6.0 |
| 执行可靠性、隔离与安全 | 6.0 |
| 报告、诊断和复现 | 5.5 |
| 工具自身测试完备性 | 7.0 |
| 设计、文档和后续维护 | 6.5 |
| 项目计划、风险和流程治理 | 6.5 |
| **平均** | **6.2** |

### 阶段门

**需求/设计：有条件通过。编码：暂缓发布。测试：有条件通过。发布：暂缓。维护：有条件通过。**

如果“必需参数必须有来源”是核心契约，修复前不得进入发布；否则需正式把它改成明确的外部输入/adapter 边界。随后修复 reports 错误处理、严格协议解析、seed 语义和可复现元数据。

## 5. Manual 方案

### 产品和交付物

Manual 的表达面最宽：函数资源模型、值域、requires/effects/success、状态机、嵌套/历史/并发状态、DFS/BFS/random/tour、多种 harness、动态库和 adapter。其自带 `make test` 通过。

静态和历史可复现问题：

- README 宣称 `--algorithm dfs`，显式输入却报 unknown algorithm；
- BFS 忽略 `--max-cases`，tour 忽略 `--max-length`；
- 状态路径生成不求 guard，可能生成不可执行路径；
- 普通执行支持父状态转换，但生成 harness 只精确匹配叶状态；nested `power_on,power_off` 可在普通执行通过、harness 失败；
- 未知 guard 变量可能静默当 true，重复声明可能被 map 覆盖；
- `test/run.sh` 的 `|| true` 会削弱失败传播。

统一 Q1–Q6 的 manual 适配首次运行会生成大量额外序列，clean Q2–Q5 失败；第二次精确序列尝试在生成/索引阶段超过预算，没有形成可接受的 24-run 证据。因此发现率为未验证。不能把未完成实验误写成工具失败，但“无法在约定预算内完成可审计触发”本身是 `[CT-测试完备性] [CT-诊断复现] [效率]` 风险。

### 工程、管理和流程资料

`ai/proposal/design/task/adr/checklists`、DSL/MBT 文档较齐全，适合继续演进；但 task 同时把 T24/T25/T28 写成完成和功能缺口，设计/README 对算法能力的宣称与实际不一致，没有集中缺陷清单、负责人、期限和变更影响记录。

### 七项评分

| 维度 | 分数 |
|---|---:|
| 模型表达和契约 | 6.0 |
| 组合生成正确性和覆盖 | 5.0 |
| 执行可靠性、隔离与安全 | 3.0 |
| 报告、诊断和复现 | 4.0 |
| 工具自身测试完备性 | 3.0 |
| 设计、文档和后续维护 | 6.0 |
| 项目计划、风险和流程治理 | 4.0 |
| **平均** | **4.4** |

### 阶段门

**需求/设计：有条件通过。编码：有条件通过。测试：暂缓。发布：不通过。维护：有条件通过。**

若嵌套状态、所有算法、max limits 和三种 harness 是公开核心能力，语义不一致构成发布阻断；若它们是扩展路线，则应拆成明确非目标/可选扩展并修正文档。下一步应先修命令选项契约、guard 生成、父状态 harness、重复定义校验和测试脚本失败传播，再完成统一 oracle 实验。

## 6. Plan/Goal 方案

### 产品和交付物

Plan/Goal 的需求、设计、任务、决策、coverage/memory/measurement 文档最集中，adapter 模式能真实传递参数和返回值；Q1–Q6 clean 12/12、mutant 12/12 报告失败，6/6 发现率为 100%。但所有 mutant 运行的工具退出码都是 0，不能作为可靠 CI gate。

关键问题：

- `src/main.cpp:269-279` 无论流程 failed/timeout/truncated 均返回 0，核心失败传播失效；
- direct `src/wise.cpp:1503-1516` 和 standalone `:1838-1862` 固定零参 ABI，standalone 参数关系只生成注释；若这些模式是公开核心能力，属于 High；
- `src/wise.cpp:187-306` 字符串式 JSON 解析拒绝合法空格/字段变体，协议契约不稳健；
- 缺少必需参数来源的模型仍可通过，adapter 发送空 args；
- max-flows 对 state/function 分别限额后拼接，整体预算可翻倍；adapter timeout 只杀单 PID；报告缺 flow/config/seed/replay 元数据；
- 函数生成器只产生每个函数一次的拓扑排列。空/子集/重复是组合扩展缺口，除非需求冻结为核心，不单独升级为 High；
- 状态流程没有完整应用 mutex/count/value 约束的证据，应与设计契约对齐。

### 工程、管理和流程资料

`ai/proposal/design/task/decisions/checklists` 和 coverage/memory/measurement 齐全，git 历史分步。问题是 T23–T29 对 standalone/参数能力的完成表述超前，checklist 缺最终签名，`make check` 用 `-` 忽略 standalone 失败，缺明确 owner/deadline/阶段阻断和统一回归入口。

### 七项评分

| 维度 | 分数 |
|---|---:|
| 模型表达和契约 | 7.0 |
| 组合生成正确性和覆盖 | 6.5 |
| 执行可靠性、隔离与安全 | 5.0 |
| 报告、诊断和复现 | 5.5 |
| 工具自身测试完备性 | 7.0 |
| 设计、文档和后续维护 | 7.0 |
| 项目计划、风险和流程治理 | 6.0 |
| **平均** | **6.3** |

### 阶段门

**需求/设计：有条件通过。编码：有条件通过。测试：有条件通过。发布：暂缓。维护：有条件通过。**

发布前必须修复失败非零退出码、direct/standalone 参数契约、严格协议解析、缺参来源校验、全局 max-flows、超时进程组清理和报告元数据；重跑 Q1–Q6，并补充真正由原生模型生成的参数/状态流程。

## 7. 软件分横向排名

| 排名 | 方案 | 平均分 | 发布状态 | 主要依据 |
|---:|---|---:|---|---|
| 1 | OMX | 8.4 | 暂缓发布，修复后最接近通过 | 核心边界清楚，状态事务/隔离/replay/管理证据完整；有严格输入边界和资料状态冲突 |
| 2 | LazyCodex | 6.2 | 暂缓发布 | 测试和执行隔离扎实，核心参数来源、报告复现和错误暴露不足 |
| 3 | Plan/Goal | 6.3 | 暂缓发布 | 文档治理强、adapter 真实传参，但失败退出码、模式不等价和协议/缺参问题更严重 |
| 4 | Manual | 4.4 | 不通过 | 功能面宽、文档较全，但 guard/隔离/失败传播存在阻断，统一 oracle 未完成 |

Plan/Goal 原始平均略高于 LazyCodex，但有更多公开执行模式的核心不等价和 High 风险，因此排在其后。Manual 的统一实验未验证，且其自身有 guard 生成/执行和隔离阻断，不能与已完成 24-run 的方案相提并论。四个方案都存在发布前工作；当前没有“无条件发布”的方案。

## 8. 共同补救和经验

### 共同补救

1. 冻结每项能力的 Required / Advertised / Optional / Unsupported 状态；
2. 将模型、生成、执行、判定、报告和退出码定义成一个端到端契约；
3. 对声明的必需参数要求唯一 producer、常量或外部输入来源，缺失在模型阶段拒绝；
4. 失败、超时、崩溃、截断和关键报告写入失败必须有稳定的非零命令退出语义，CI/阶段门不得忽略；
5. 明确逐步进程的状态持久化、snapshot/restore 或外部状态桥，并把桥接成本单独计入；
6. 超时清理进程组和后代，严格解析版本化协议和数值 token；
7. 统一记录模型/配置/seed/环境/flow ID/expected/actual/trace/replay/hash；
8. 将 Q1–Q6 控制组和项目适配资产接入各自可重复回归，但不把适配器结果冒充原生生成能力；
9. 对 coverage 说明父子进程数据归集策略，不能把未验证的低数字或高数字当成完整质量证明；
10. 把过时文档、待办、负责人、期限和 release gate 对齐。

### 经验反馈

- 供应触发的 mutant 只证明执行器判错；生成器自动发现必须单独测量；
- 每步启动新进程的工具需要公开状态契约，否则有状态产品会被适配器外部文件“修饰”；
- “命令返回 0”不是“产品通过”；失败传播必须由 shell、CI 和阶段门共同核对；
- 需求、设计、测试和发布状态需要一条可机器检查的追踪链，不能让历史待办和最终 PASS 并存；
- 领域专项标准应记录从复评发现的通用门禁，再回写通用标准，而不是把所有测试工具特征推广给软件项目。

## 9. AI 附件补充评价

本次启用 [评价标准-AI附件.md](/home/workspace_data/works/myprojects/组合测试评价/评价标准-AI附件.md)，原因是四个项目明确使用了不同 AI 编排方式，且比较目标包含 AI 交付效率和后续可维护性。AI 评价只针对本次可观察证据；普通 CLI、JSON、replay 或构建接口不自动计为 AI 专用接口。

AI 证据分别由独立 subagent 复核，报告在 [AI 复评资产目录](/home/workspace_data/works/myprojects/组合测试评价/wise_combine_test-review-2-assets/ai)；缺少模型/提示/人工控制/成本基线时按附件规则封顶或标为未知。

| 方案 | AI贡献效果 | AI可延续资料 | AI专用接口 | AI质量与安全 | AI成本与持续性 | S_AI | S_final = 0.85×软件分 + 0.15×S_AI |
|---|---:|---:|---:|---:|---:|---:|---:|
| OMX | 5.0 | 8.0 | 3.0 | 5.0 | 4.0 | **5.0** | **7.89** |
| LazyCodex | 5.0 | 5.5 | 2.0 | 5.0 | 4.0 | **4.3** | **5.92** |
| Manual | 4.0 | 7.0 | 2.0 | 3.0 | 2.0 | **3.6** | **4.28** |
| Plan/Goal | 5.0 | 7.5 | 2.0 | 4.0 | 3.5 | **4.4** | **6.02** |

共同结论：四个项目都有供 AI 或人工继续阅读的需求/设计/任务资料，但没有充分证据证明 AI 净效率收益；未看到带来源、权限、版本和新鲜度控制的 RAG 或专用知识查询接口；模型、提示、调用量、人工复核、成本和返工台账普遍不完整。OMX 的迭代 ledger/evidence 最接近可延续资料要求，Manual 和 Plan/Goal 的工程资料较完整但缺 AI provenance，LazyCodex 的 `.agents` 被忽略且知识库过期。AI 分不能抵消软件 High 问题。

软件分的风险排序将 LazyCodex 放在 Plan/Goal 前面，是因为 Plan/Goal 暴露出更多公开执行模式的核心不等价和 High 风险；这不是简单按平均分降序排列。启用 AI 附件并按 85% 软件分 + 15% AI 分计算后，数值排名为 OMX、Plan/Goal、LazyCodex、Manual：Plan/Goal 为 6.02，略高于 LazyCodex 的 5.92。这个数值排序不改变阶段门结论：Plan/Goal 仍因失败退出码、direct/standalone 契约和协议/缺参问题暂缓发布，四个项目当前都没有无条件发布资格。

## 10. 启用 AI 附件后的最终排名

| 排名 | 方案 | 软件分 | AI 分 | 最终分 | 阶段门结论 |
|---:|---|---:|---:|---:|---|
| 1 | OMX | 8.4 | 5.0 | **7.89** | 暂缓发布，修复后最接近通过 |
| 2 | Plan/Goal | 6.3 | 4.4 | **6.02** | 暂缓发布 |
| 3 | LazyCodex | 6.2 | 4.3 | **5.92** | 暂缓发布 |
| 4 | Manual | 4.4 | 3.6 | **4.28** | 不通过 |

最终分只用于本次方案比较；软件 High 风险、未满足的 Required 能力和发布阶段门仍然优先，AI 分不能抵消这些问题。

AI 阶段门补救：建立模型/提示/输入资料/输出/人工批准/成本台账；将技术调查、ADR、fixture、失败报告和回归命令建立稳定索引；为需要 AI 继续开发的项目提供带版本、来源、权限和错误状态的查询接口；对模型或供应商升级建立回归和离线复现路径。没有这些证据时，AI 维度只能作部分验证，不能声称某种 AI 编排普遍更优。

下次复评条件：完成各自软件补救和 AI 台账/资料/接口补救，更新产品/工程/管理/流程证据，重新运行 Q1–Q6 及对应生成 probes，并按新增标准版本记录差异。
