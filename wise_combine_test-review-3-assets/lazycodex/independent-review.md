# LazyCodex 独立复评（wise_combine_test-review-2）

评审对象：`wise_combine_test.lazycodex`，当前 `layzcodex` 分支 `52a7fa5`，Linux，CMake Debug 与 ASan/UBSan/LSan 构建。依据《评价标准-组合测试项目.md》及冻结的 Q1–Q6 资产。

## 证据

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel`：通过。
- `ctest --test-dir build --output-on-failure`：19/19 通过；`ctest --test-dir build-asan --output-on-failure`：19/19 通过。直接在源目录运行 `ctest` 会得到 0 tests，这是调用目录不正确；从 build 目录运行才是有效证据。
- `run_review.sh` 对 Q1–Q6 的 clean（MUTANT=0）和对应单缺陷 mutant 各重复两次，共 24 runs。12/12 clean 通过，12/12 matching mutant 被判为失败；冻结 oracle 的缺陷发现率为 6/6 = 100%，正常用例误报率为 0/12 = 0%。每个模型生成 1 条线性流程，重复运行序列一致。详情见 `results.csv`、`run-log.md` 和 `reports/`。
- Q1–Q6 通过 `expect.state` 比较适配器传回的原始返回值（如 `r11`、`r-2`）。因为原生 runner 每一步启动一个进程，适配器用私有 state 文件保存 `QueueState`；这是必要的额外集成成本，适配器本身不写期望值断言。
- 生成探针：将 relation fixture 的 seed 从 9 改为 10，生成的 flow 序列不变，说明 `seed` 当前未参与生成（`src/generate/generate.cpp:76`）。`max_cases=0` 正确产生 `case_limit` 和退出码 3。
- 将 `--reports` 指向已有普通文件时，CLI 在 `std::filesystem::create_directories` 抛异常并以 134 终止，没有结构化错误或报告（`src/cli/cli.cpp:174`）。

## 四层评审

### 1. 产品和交付物

`[功能完整性] [CT-表达能力]`：JSON 可描述状态、迁移、函数签名、参数/返回类型、argument 和 before 关系；语义校验能拒绝未知引用、类型不匹配、重复绑定和 ordering cycle。Q1–Q6 的状态路径和返回值断言可运行，但只能把返回值翻译成 `observed_state`，不能在模型中直接声明返回值 oracle，也没有队列模型所需的参数/资源/guard/mutex/parallel/count/value 语义。

`[CT-生成正确性]`：生成器遵守状态边、before/argument 前置关系、max cases/steps，并能处理自环。它是按 ID 排序的确定性 DFS；`seed` 被忽略，未提供随机策略、约束覆盖、未覆盖原因或多策略选择。重复调用只对自环有特殊处理，非自环不能重走；统一 Q1–Q6 没有覆盖这一限制。

`[CT-执行可靠性] [CT-隔离安全]`：`execve`、固定环境、进程组、单步/总超时和输出上限均有测试，Q1–Q6 以及 runtime failure matrix 通过。产品缺陷：报告目录创建失败会直接异常 abort（上述复现，High）；协议解析是字符串搜索而非完整 JSON 解析，虽能拒绝 malformed/extra fixture，但对转义、字段顺序和复杂合法 JSON 的鲁棒性未证实。

`[CT-诊断复现] [CT-可观测性]`：有 JSON/text step reports、退出码、stderr、状态和测量字段；但报告缺少模型内容/版本、seed、环境、完整 args、期望值、trace、哈希和 replay/tamper 校验，难以单凭报告复现复杂失败。

### 2. 工程资料

`AGENTS.md` 给出核心必须/可选要求，README 与 README.zh.md 记录 schema、CLI、adapter、安全和 sanitizer；`.omo/plans`、`.omo/evidence` 保存分阶段计划、验收和决策痕迹。工程分层（model/spec/generate/runtime/report/cli）清楚，CTest 覆盖正常/非法模型、关系、超时、崩溃、输出上限和 CLI。

资料缺口：需求没有将 guard/mutex/parallel/count/value、返回值 oracle、覆盖定义、seed 语义写成可验收契约；设计没有独立的 architecture/design 文档或方案比较，主要依赖计划和源码；测试矩阵没有覆盖模型语义 × 生成策略 × 执行模式的完整组合。`AGENTS.md` 中的 PROJECT KNOWLEDGE BASE 仍写着“requirements-only/no implementation”，与当前实现不一致，降低维护可信度。

### 3. 项目管理资料

`.omo/plans/wise-combine-test.md` 有范围、依赖矩阵、分阶段任务、验收命令、风险和 commit 策略；`.omo/evidence` 有各阶段日志和最终审查，满足可追踪性基础。成本、AI 调用量、人工时间、计划偏差和资源预算没有量化记录；风险清单没有把“报告目录失败 abort”“seed 未生效”“协议解析非完整 JSON”列为未决风险。

### 4. 流程治理和变更

commit 历史按模型、生成、runtime、CLI/docs 分阶段组织，工作树干净；中英文 README 同步规则明确，sanitizer 和最终审查证据可追溯。缺少自动化的文档/需求追踪检查、变更影响矩阵和发布/回滚记录；当前 Q1–Q6 适配资产位于评审目录，不是产品回归套件，未来变更仍需人工重新接入。

## 问题清单

### High

1. **报告目录错误导致进程异常终止。** `src/cli/cli.cpp:174` 未检查 `create_directories` 异常；`--reports` 指向普通文件时以 SIGABRT/134 退出，调用者拿不到稳定退出码、结构化报告或诊断。这违反 `[CT-执行可靠性] [可靠性]` 的显式失败要求。修复：捕获 filesystem 异常，返回文档化 runtime/usage 错误并写 stderr；增加不可写目录/普通文件 fixture。

### Medium

2. **seed 参数没有语义。** `src/generate/generate.cpp:76` 明确丢弃 seed；README/AGENTS 将其列为配置和可复现输入，但不同 seed 不产生任何差异，也没有声明“确定性排序、不使用 seed”的边界。修复：实现声明的随机策略，或删除/明确冻结 seed 语义并补测试。

3. **组合表达能力明显低于专项目标。** 当前仅有状态边和两类关系；guard、mutex、parallel、count/value/state、资源生命周期和负向流程无法表达。Q1–Q6 可通过线性状态别名映射，但不能证明这些缺失语义的组合测试能力。修复：补需求/设计边界；若属于范围，增加 schema、validator、generator 和 oracle 矩阵，否则在文档中明确“不支持”。

4. **报告/协议复现信息不足。** `src/report/report.cpp:7-14` 只写 flow、step、function、status、observed_state、stderr 和 exit；缺少 args、expected、seed、模型/配置摘要、环境、稳定报告 ID、trace/replay/hash。修复：扩展结构化报告并加入 replay 与篡改/版本校验测试。

### Low

5. **协议解析采用脆弱字符串扫描。** `src/runtime/runtime.cpp:54-97,147` 不能作为完整 JSON 解析器处理所有合法空白、转义和嵌套值；当前 fixture 只证明简单对象、malformed 和额外字段。修复：使用严格 JSON parser 或收紧并记录协议语法，补合法复杂 JSON 及 Unicode/转义测试。

6. **项目知识库文档过期。** `AGENTS.md:46-109` 仍描述“requirements-only/no implementation”，与源码、README 和 CMake 不一致。修复：在同一变更中更新结构、命令和证据指针。

## 七项评分

| 维度 | 分数 | 依据 |
|---|---:|---|
| 模型表达和契约 | 6.0 | 基础状态/函数/参数/两类关系可用；专项约束和返回值契约缺失 |
| 组合生成正确性和覆盖 | 6.0 | 有界 DFS、状态/before/argument 正确；seed、策略和广泛约束缺失 |
| 执行可靠性、隔离与安全 | 6.0 | 超时/崩溃/输出/进程隔离证据充分；报告目录异常为 High |
| 报告、诊断和复现 | 5.5 | 有结构化 step 报告和测量；缺 trace/replay、配置/环境/expected |
| 工具自身测试完备性 | 7.0 | 19/19 Debug + 19/19 sanitizer；Q1–Q6 发现率 100%，但组合矩阵不全 |
| 设计、文档和后续维护 | 6.5 | 分层实现、README 双语、计划/evidence 完整；KB 过时、专项边界未闭合 |
| 项目计划、风险和流程治理 | 6.5 | 计划/依赖/commit/evidence 可追踪；成本、偏差、变更影响和发布回滚不足 |

原始平均分 6.2。因存在未缓解 High，按硬门槛不得无条件通过或列为首选；可靠性、测试和总体结论均应标记为有条件。

## 阶段门和补救

- **需求/设计：有条件通过。** 基础 schema 和执行边界已有证据；必须补齐专项不支持清单、返回值 oracle、seed 语义、约束覆盖矩阵和设计替代方案。
- **编码：暂缓进入发布。** 先修复 reports 路径异常退出，增加回归测试；补充严格协议解析或明确协议边界。
- **测试/验收：有条件通过。** 统一 Q1–Q6 为 24 runs、6/6 发现；这是 supplied-trigger 结果，不是自动发现或穷举覆盖证明。需增加非自环重走、子集/重复、参数传递、复杂 JSON、报告失败和多约束场景。
- **发布：不通过。** High 问题未缓解，且报告无法独立复现完整配置。
- **维护：有条件通过。** 分层代码便于修改，但必须更新过期 AGENTS/知识库，并把 Q1–Q6 资产纳入可重复回归入口。

首要补救任务：修复目录错误处理（High）；明确或实现 seed 与约束语义；升级报告/replay 元数据；同步知识库和需求矩阵；将 Q1–Q6 及扩展场景接入产品回归。经验教训是：`ctest` 必须从 build 目录运行，统一 oracle 需要独立适配层且不能依赖产品自有断言，跨进程有状态被测对象会增加集成成本。
