# LazyCodex 方案第四轮独立评审报告（wise_combine_test）

- 评审对象：`/home/workspace_data/works/myprojects/wise_combine_test.lazycodex`（分支 `layzcodex`）
- 评审标准：《评价标准.md》v2.2（2026-09-12）、《评价标准-AI附件.md》v1.0、《评价标准-组合测试项目.md》
- 评审日期：2026-09-15；上一轮（review-3）基线：`52a7fa5`（2026-09-12）
- 本轮证据资产：`wise_combine_test-review-4-assets/lazycodex/`（24-run 结果、探针模型、probe 日志、报告样本）
- 纪律声明：未修改被评项目任何源码；`git status` 评审前后均为空（0 行差异）；结束后确认无残留 oracle/adapter 子进程。

---

## 1. 版本快照

| 项 | 值 | 验证方式 |
|---|---|---|
| git HEAD | `b67e24f7d4da1bf36afa1f337abf939a8a50689b`（分支 `layzcodex`） | `git rev-parse HEAD`，已验证 |
| git status | 干净（porcelain 无输出，评审前后一致） | 已验证 |
| 52a7fa5..b67e24f | 89 个提交（含补救 + evidence/chore 提交） | `git log --oneline`，已验证 |
| diff 规模 | `76 files changed, 5112 insertions(+), 73 deletions(-)` | `git diff --stat \| tail -3`，已验证 |
| 关键补救提交 | `3627992 fix(model): reject missing transition arguments`、`215c931 fix(generator): enforce argument prerequisites`、`2871ce6 feat(generate): make seed influence deterministic choices`、`acc1c4d feat(generate): permit bounded nonself cycles`、`ddc4f56 feat(runtime): parse adapter responses with strict json`、`0c54f45 fix(cli): reject unusable reports paths`、`1468a37 feat(cli): add report v2 replay`、`d04e650 fix(replay): harden integrity and output paths` 等 | 已验证 |
| g++ | 9.4.0 (Ubuntu 20.04) | 已验证 |
| cmake | 3.16.3；内核 5.15.0-139 | 已验证 |

---

## 2. review-3 遗留问题逐项核验

### a. 必需参数缺来源未在模型阶段拒绝 —— **fixed（模型阶段拒绝，已验证）**

- 代码：`src/model/model.cpp:180-191` 在 `Model::validate()` 中对每个 transition 的每个 function 参数检查"有字面量 `args` 或有 argument relation 绑定"，缺失抛 `ModelError(Code::invalid_argument)`；`src/spec/spec.cpp:57` 在**规格解析（模型阶段）**即调用 `validate()` 并包装为带稳定错误码的诊断。
- 实测：`wise-combine validate missing_param.json` → `spec error: semantic validation failed [invalid_argument]: transition is missing a value for parameter: t1.p`，**exit 2**；`run` 同样在解析阶段拒绝（exit 2），不启动任何子进程。
- 阶段归属判定：虽然存在提交 "fix(generator): enforce argument prerequisites"（`generate.cpp:85` 生成前再次 `model.validate()`，属第二道防线），主拒绝点在 **model/spec 阶段**，满足专项标准"来源缺失必须在模型阶段拒绝"。错误码集合（`model.cpp:63-75`）提供 8 个稳定 code name（`a1646d4`）。

### b. seed 被丢弃 —— **fixed（seed 真实参与生成，已验证）**

- 代码：`src/generate/generate.cpp:87`（`std::mt19937_64(seed)`）与 `:50-53`（候选按 ID 排序后用 rng 洗牌）。
- 实测（分支模型，两候选 t_a/t_b）：`seed=1 → [t_b],[t_a,t_c]`；`seed=2/3/7 → [t_a,t_c],[t_b]`；`seed=1` 重复运行结果完全一致。README 冻结语义："相同规范+seed 产出相同流程，不同 seed 在存在多候选时可产生不同顺序"。单候选模型（Q1–Q6）结果不随 seed 变化，符合文档。

### c. 非自环重走 / 约束语义 / 负向 / 复杂返回值 —— **按能力分类分别判定**

| 能力 | 分类 | 本轮证据 | 结论 |
|---|---|---|---|
| 非自环 transition 重走 | 核心契约（README 声明） | `acc1c4d`；实测 A↔B 循环模型生成 8 步重走流程 `go_b,go_a,...`，`status=step_limit` exit 3 | **fixed，已验证** |
| 自环重复调用 | 核心契约 | 实测自环模型生成 9 条不同重复长度流程 | 已验证（原生自动产生） |
| 空流程 | 核心契约（README 声明） | 实测无 transition 模型产出 1 条空流程（`flow_id=""`）exit 0 | fixed（`b29a8e0`），已验证 |
| guard/mutex/parallel/count/value、资源生命周期、多对象、负向流程 | **明确非目标** | `docs/adr/0004-advanced-constraints-boundary.md`（已接受，含进入条件）+ `docs/requirements-matrix.md` CT-011 | 边界已正式冻结，不再计为缺陷；不得在文档中暗示已实现 |
| 复杂返回值 oracle | 部分支持（文档化边界） | 数值 producer→consumer 实测（returns `n:42` → 注入 consumer `args v:42` 并入报告）；但 `expect` 仅支持字符串 state 断言，返回值本身无 expected 断言（ADR 0005 记录边界） | 部分验证，按声明边界处理 |

### d. adapter 字符串扫描解析 JSON —— **fixed（严格 RFC 8259 解析，已验证）**

- 代码：`src/spec/spec.cpp:19-34` 完整递归下降 JSON 解析器（空白、`\uXXXX` 含代理对、重复键拒绝、尾随数据拒绝）；`parse_adapter_response`（`spec.cpp:83-109`）strict_keys 拒绝未知字段、类型校验、未知 status 拒绝。`runtime.cpp:149` 优先走该解析器。
- 实测（自构 adapter，路径满足 allowlist）：
  - 合法变体（换行+缩进、字段换序、转义引号/反斜杠、`caf\u00e9` Unicode）→ **接受，exit 0**；
  - 重复 `protocol` 字段 / 尾随 `extra` / 未知 status `weird` / `observed_state` 为数字 → 全部 `protocol_error`，**exit 5**。
- 结论：review-3 的 Low-5 已修复并有产品回归（`runtime_formatted/unicode/escaped/duplicate_status`）。遗留：`runtime.cpp:165-168` 保留了一段不可达的旧字符串扫描分支（见第 6 节新问题 N1）。

### e. `--reports` 普通文件 → 异常 134 —— **fixed（exit 5 + 稳定诊断，已验证）**

- 代码：`src/cli/cli.cpp:197-203` 改用 `error_code` 重载，失败写 stderr 并返回 `kRuntimeFailure(5)`；报告逐文件写失败（`cli.cpp:217-221`）与 summary 打开/flush 失败（`cli.cpp:227-242`）均检查。
- 实测：`--reports /tmp/lazy4/report-file`（普通文件）→ `unable to create reports directory '...': Not a directory`，**exit 5**；不可写根路径 → `Permission denied`，exit 5。产品回归 `cli_reports_error` + `report_write_failure`（/dev/full 符号链接逐文件注入，断言 exit 5、stdout 为空、stderr 有诊断）。

### f. step report 缺 args/expected/seed/模型/环境/trace/replay/hash —— **基本补齐（已验证）**

实测一份 `run` 报告（relation fixture）：
- v1 每步：`index/transition/function/status/args(含关系注入值)/observed_state/expected_state/stderr/exit_status/detail`——step1 `args {"input":"from-producer"}` 证明 effective args 入报告；
- summary：`schema_version/generation_status/case_count/passed/failed/wall/cpu/peak_rss/seed`；
- v2 envelope（SHA-256）：`model`（完整 canonical 规范，含 seed/limits/relations）、`generator{strategy:seeded-dfs-v1, termination_status}`、`flow`、`adapter{path,sha256,arguments,working_directory}`、`runtime{step/total_timeout, output_limit, environment:[PATH,LC_ALL]}`、`result`（含 `returns`）。
- 配套命令实测：`verify-report`/`verify-report-v2` exit 0；**篡改 payload 后 digest 不匹配 → exit 2**；`replay` 重放保存流程 exit 0 并另写 v1/TXT/v2；`hash-report` 输出摘要。replay 防覆盖（拒绝已存在路径、symlink/hardlink 别名、输入输出同目录）与 adapter 摘要比对在 `src/replay/replay.cpp:51-94`。
- 仍未覆盖（文档化）：replay 不恢复外部 adapter 状态、SHA-256 非来源认证、无主机环境采集（固定环境即契约）。判：**fixed（带文档化边界）**。

### g. `.agents` 被忽略 vs AGENTS.md 要求追踪、KB 过期 —— **partial**

- KB 已修复：`AGENTS.md:46-154` PROJECT KNOWLEDGE BASE 已刷新为当前实现（`f2b7ff1`，Updated 2026-09-12/commit 8a19d50），结构、命令、边界与源码一致。
- `.agents` 矛盾未解决：`AGENTS.md:20`（[必须]）与 KB CONVENTIONS 仍要求"每步提交包括 `.agents/` 的文件"，而 `.gitignore:70` 仍整体忽略 `.agents/`，`git ls-files .agents` 为空。且 `.agents/` 实际含 `auth.json`、sqlite 会话库等凭证/运行态文件——按字面执行会泄漏凭证，需把要求细化为"提交指导性文件、排除凭证与运行态"（见 N3）。

---

## 3. 项目自身测试套件

- 入口（README）：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel && (cd build && ctest --output-on-failure)`；ASan/UBSan/LSan：`-DWISE_COMBINE_ENABLE_SANITIZERS=ON`。
- 实测结果：
  - Debug：配置+构建成功，`ctest` **45/45 通过，exit 0**（总耗时 1.57s）；
  - ASan/UBSan/LSan：构建成功，`ctest` **45/45 通过，exit 0**（6.40s）；
  - 覆盖率门禁：`build-coverage` + `ctest` 45/45 + `coverage-check` target → **project source coverage: 84%（1169 可执行行）≥ 80% 阈值，通过**。
- 测试数从 review-3 的 19 增至 45，新增覆盖：evaluation_q1–q6（冻结 oracle 产品化回归）、runtime_formatted/unicode/escaped/duplicate_status/unknown_status/allowlist、cli_reports_error/report_write_failure/run_id_error/verify_report_invalid/verify_summary/hash_report/wrap_report_v2/verify_report_v2/replay_v2/help。
- 测试脚本无 `|| true` 吞失败；预期失败均显式断言退出码（已核验 `run_case.cmake`、`cli_reports_error_test.sh`、`report_write_failure.cmake`）。

---

## 4. 统一 oracle 重跑（Q1–Q6）

- 脚本：review-3 冻结的 `run_review.sh`，产物在 review-4-assets。**两处适配（披露）**：
  1. `COMMON` 由不存在的 `wise_combine_test-review-2-assets/common` 改指 `wise_combine_test-review-3-assets/common`（内容为同一冻结件：`queue_sut.h`/`cases.json` SHA256 与 SHA256SUMS 一致，control-results.json 含 24 条对照记录）；
  2. 每个运行包一层 `timeout -k 1 10`，落实冻结预算 10s/进程（实际单 run ~0.1s，全程 2.3s）。
- 状态桥披露（与 review-3 相同）：产品每步启动新 adapter 进程且不跨步骤保存 SUT 状态，评审 adapter 用私有 `--state` 文件保存/恢复 `QueueState`（`queue_adapter.cpp`）；adapter 断言仅翻译原始返回值为 `observed_state`，期望值全部由产品模型 `expect.state` 表达，每场景间重置状态文件。
- 结果（24 runs）：

| 指标 | 结果 |
|---|---|
| clean 对照（Q1–Q6 × 2） | **12/12 通过**，exit 0，`passed=1/failed=0` |
| matching mutant（Q1–Q6 × 2） | **12/12 正确判失败**，exit 4（mismatch），非崩溃 |
| 缺陷种类发现率 | **6/6 = 100%** |
| 正常用例误报率 | **0/12 = 0%** |
| 抽查判定质量 | Q2-mutant 报告：`state mismatch: expected r11, observed r22`（真实 FIFO/LIFO 差异）；Q5：`expected r0, observed r1`（真实计数差异） |

- 与 review-3 一致：这是**供应触发流程的执行判定证据**，不证明生成器自动发现；生成器自动产生能力另见第 5 节。
- 额外正发现：项目已把冻结 oracle 原样（q1–q6.json、queue_sut.h 与评审资产逐字节一致，diff 验证）注册为产品回归 `evaluation_q1..evaluation_q6`（`tests/evaluation/run_case.cmake`：clean 期望 0、mutant 期望 4，运行后校验报告），弥补了 review-3 "适配资产不在产品回归内"的缺口。

---

## 5. 生成 probe（原生生成器自动产生能力）

| Probe | 模型/命令 | 结果 | 对象分类 |
|---|---|---|---|
| 非自环重走 | A↔B 循环，max_steps=8 | 原生产生 8 步重走流程，`step_limit` exit 3 | state cycle |
| 自环重复 | 自环+出口，max_cases=10 | 原生产生 9 条不同重复长度流程（含不同深度） | repeat |
| 空流程 | 无 transitions，max_cases=3 | 原生产生 1 条空流程 exit 0；max_cases=0 → 0 条 `case_limit` exit 3 | empty |
| 子集/前缀 | 链 a→b→c | **仅产生完整路径 [t1,t2]，无 [t1] 前缀/子集**（DFS 只在 dead-end/step-limit 发射） | subset——缺口，未在需求矩阵归类 |
| 限额 | max_cases=0 / max_steps=8 | 均生效并有区分退出码 3 | limits |
| seed 差异 | 分支模型 seed=1/2/3/7 | 多候选时顺序不同；同 seed 复现一致 | 策略选项 |
| before 顺序 + 重复 | loop 自环 before use | `loop` 出现后 `use` 不会再出现 `loop`（`loop*,use,back,use,...`），语义正确 | 约束过滤 |
| 参数传递（字面量） | int/string/bool/number | adapter 请求与报告均含 `{"b":true,"n":1.5,"s":"hi","x":7}` | 参数传递 |
| 参数传递（关系） | 数值 returns n=42 | 注入 consumer 请求 `{"v":42}` 并入 v2 报告 | relation DAG |

---

## 6. 新发现问题

| 编号 | 严重度 | 标签 | 问题 | 证据 |
|---|---|---|---|---|
| N1 | Low | `[CT-扩展维护] [可维护性]` | `src/runtime/runtime.cpp:165-168` 保留不可达的旧字符串扫描解析分支（严格解析器在先、异常已转 protocol_error，两条路径条件互斥），易误导后续维护 | 代码推演 + 全部协议测试走 strict parser |
| N2 | Low | `[CT-执行可靠性] [可靠性]` | SIGPIPE 未显式忽略：`runtime.cpp:130` `(void)!write(...)` 忽略写结果；若子进程先退出关闭管道读端，父进程理论上会被 SIGPIPE 终止（exit 141）。实测快速退出 adapter 20 次未复现（payload 小、写入先完成，稳定 exit 5/protocol_error），属理论竞态 | 20 次压力实测 + 代码 |
| N3 | Low-Medium | `[AI-可延续性] [可维护性] [合规性]` | `AGENTS.md:20` [必须]要求提交 `.agents/` 文件 vs `.gitignore:70` 整体忽略 `.agents/` 的矛盾未解决；且 `.agents/` 含 `auth.json`/sqlite 凭证运行态，按字面追踪会泄漏凭证 | git check-ignore + ls |
| N4 | Low | `[CT-生成正确性] [CT-可观测性]` | 子集/前缀流程不生成，且该能力既未在需求矩阵列为承诺也未列为非目标（ADR 0004 只冻结 guard 等）；应按专项标准补归类，避免"未冻结状态" | chain probe 实测 |
| N5 | Low | `[CT-测试完备性] [可观测性]` | 覆盖率口径漂移：roadmap 写"85%（667 行）"，本轮实测 84%（1169 可执行行，gcov 行口径）；门槛 80% 通过，但口径/时点未随提交同步；无 fuzz/性能剖面测试 | coverage-check 实测 |

无 High、无 Medium（review-3 的唯一 High 与四项 Medium 全部 fixed 或按 ADR 重分类）。

---

## 7. AI 附件五项评价（0-10）

启用原因：项目由 AI 编排（LazyCodex/ulw 循环）开发，`.omo` 为 AI 延续资料目标。

| 维度 | 分 | 依据 |
|---|---:|---|
| AI 贡献效果 | 5.0 | 两轮评审间 89 提交、~5100 行，全部 review-3 发现均有 test-first 修复与 evidence（`blocker-red.log` 先失败证据、gate-review SHA 绑定、quality-gate.json 结构化 artifact 引用，抽查真实可追溯）；但**无模型名/版本、无人工控制点、无时间/token 基线**，按附件硬顶 ≤5 |
| AI 可延续资料 | 7.0 | KB 已刷新；新增 architecture.md、requirements-matrix（12 项含明确非目标）、test-matrix、3 份 ADR、improvement-roadmap（评审发现→commit→验收条件逐项映射）；冻结 Q1–Q6 纳入产品回归并被本轮独立复跑证实——资料"被后续任务实际复用"有实证；扣分：`.agents` 矛盾、evidence 路径深且无统一索引 |
| AI 专用接口 | 2.5 | 仍无 RAG/代码索引/AI 查询接口；稳定 JSON CLI + 自描述 v2 报告（schema_version/摘要/元数据）较上轮更好，但非 AI 专用、无来源/权限/新鲜度语义 |
| AI 质量与安全 | 6.0 | 代码审查+QA 双轨 gate、blocker 追踪到解决、ASan+覆盖率门禁、allowlist/固定环境/execve 无 shell/replay 防覆盖、明示不采集凭证；扣分：模型 provenance/提示词未记录，无法换模型重验证 |
| AI 成本与持续性 | 3.0 | 仅 `spawn-count.json`（{"count":13}）一项成本痕迹；无 token/费用/人工时间/供应商替代与离线复现记录 |

S_AI = (5.0+7.0+2.5+6.0+3.0)/5 = **4.7**（review-3 为 4.3）。

---

## 8. 七项软件评分

| 维度 | 分数 | 一句理由 |
|---|---:|---|
| ① 模型表达和契约 | 8.0 | 核心契约（状态/typed 函数/argument/before + 8 类稳定错误码 + 参数来源模型阶段拒绝）完整且有实测；guard/mutex/parallel/count/value/负向按 ADR 0004 冻结为明确非目标，返回值断言仅经 state 别名 |
| ② 组合生成正确性和覆盖 | 7.0 | seeded-DFS 在契约内正确（seed 复现/差异、非自环重走、空流程、限额、before-on-repeat 均实测），但子集/前缀不生成、无负向生成、无约束覆盖/未覆盖原因度量 |
| ③ 执行可靠性、隔离与安全 | 8.0 | execve+固定环境+进程组+allowlist+双超时+输出上限+严格 RFC8259 协议（含对抗实测）+报告写失败 exit 5；24-run 零误报；遗留仅 SIGPIPE 理论竞态与死代码两个 Low |
| ④ 报告、诊断和复现 | 8.0 | v1/v2 报告字段齐（args/expected/seed/模型/adapter sha256/环境/returns），verify/hash/replay 与篡改检测实测有效；缺全局稳定 run ID、外部状态不恢复（均已文档化） |
| ⑤ 工具自身测试完备性 | 8.0 | 45/45 Debug + 45/45 ASan/UBSan/LSan + 覆盖率门禁 84% + 冻结 oracle 产品化回归 + /dev/full 与协议攻击矩阵；缺 fuzz 与生成覆盖度量 |
| ⑥ 设计、文档和后续维护 | 7.5 | architecture/requirements-matrix/test-matrix/ADR/双语 README 闭环，分层清晰；`.agents` 追踪矛盾未解、设计替代方案比较仍偏简 |
| ⑦ 项目计划、风险和流程治理 | 7.5 | 评审→roadmap→commit→evidence 的整改闭环真实可追溯（本轮逐项证实）；成本/AI 台账、计划偏差、发布回滚记录仍缺 |

**平均 S_software = 54/7 ≈ 7.7**（review-3 为 6.2）。无未缓解 High，硬门槛（核心需求、可靠性关键场景、独立 oracle、失败退出码）均满足。
S_final = 0.85×7.7 + 0.15×4.7 = **7.25**。

---

## 9. 阶段门结论

| 阶段 | 结论 | 一句依据 |
|---|---|---|
| 需求 | **通过** | 需求矩阵 12 项编号+验收证据+oracle+明确非目标（ADR 0004）已冻结 |
| 设计 | **通过** | architecture.md+ADR 0003/0004/0005 覆盖数据流、失败路径、退出码与报告契约，边界闭合 |
| 编码 | **通过** | parser/validator/generator/runtime/report/cli 回归 45/45（Debug+ASan），无未缓解 High |
| 测试 | **有条件通过** | Q1–Q6 6/6、0 误报、协议/写故障/tamper 矩阵齐备；条件：子集/负向归类入需求矩阵、补生成覆盖度量、清理 N1/N2 |
| 发布 | **有条件通过** | 干净构建可复现、退出码稳定、限制说明完整；条件：补发布说明与回滚验证记录，roadmap P0 遗留场景（权限不足报告路径等）纳入清单 |
| 维护 | **有条件通过** | 变更影响通过 ADR+matrix+回归受控、oracle 已入产品回归；条件：解决 `.agents` 追踪矛盾（区分凭证与指导文件）、补成本台账 |

---

## 10. 证据覆盖率

分母（适用检查项 39 项，要点）：模型表达×6（状态/类型/argument/before/拒绝语义/参数来源）、生成×8（seed/非自环/自环/空/子集/限额/前置过滤/合法only）、执行×8（协议严格解析/状态分类/非零退出/超时/输出上限/allowlist/隔离环境/参数传递）、状态桥披露×1、报告×5（v1 字段/v2 字段/verify+replay+篡改/写失败/reports 路径）、SIGPIPE×1、工具测试×4（Debug/ASan/覆盖率/Q1-Q6 回归）、fuzz×1、工程文档×3（矩阵/架构/双语同步）、管理×2（计划 evidence/成本台账）、流程×2（评审闭环/.agents 一致性）。
不计入分母（经说明的明确非目标）：guard/mutex/parallel/count/value、负向流程、replay 外部状态恢复与来源认证（ADR 0004/0005）。

- 有充分证据判为通过或失败：**37/39 = 94.9%**（其中已证实缺口/失败 4 项：子集不生成、SIGPIPE 竞态、成本台账缺失、.agents 矛盾；通过 33 项）。
- 未验证不计入：fuzz 验证、发布回滚演练（2 项）。
- 通过率（另列）：33/37 = 89.2%。

---

## 附表一：七项软件分数

| 维度 | review-3 | 本轮 | Δ |
|---|---:|---:|---:|
| 模型表达和契约 | 6.0 | **8.0** | +2.0 |
| 组合生成正确性和覆盖 | 6.0 | **7.0** | +1.0 |
| 执行可靠性、隔离与安全 | 6.0 | **8.0** | +2.0 |
| 报告、诊断和复现 | 5.5 | **8.0** | +2.5 |
| 工具自身测试完备性 | 7.0 | **8.0** | +1.0 |
| 设计、文档和后续维护 | 6.5 | **7.5** | +1.0 |
| 项目计划、风险和流程治理 | 6.5 | **7.5** | +1.0 |
| **平均** | **6.2** | **7.7** | **+1.5** |

## 附表二：AI 附件五项分数

| 维度 | review-3 | 本轮 |
|---|---:|---:|
| AI 贡献效果 | 5.0 | **5.0**（质量证据强但无模型/人工控制/成本基线，按附件封顶） |
| AI 可延续资料 | 5.5 | **7.0** |
| AI 专用接口 | 2.0 | **2.5** |
| AI 质量与安全 | 5.0 | **6.0** |
| AI 成本与持续性 | 4.0 | **3.0**（本轮细查后确认仅 spawn-count 一项痕迹） |
| **S_AI** | **4.3** | **4.7** |

S_final = 0.85×7.7 + 0.15×4.7 = **7.25**。

## 附表三：24-run 统一 oracle 结果

| 场景 | clean r1 | clean r2 | mutant r1 | mutant r2 | mutant 退出码 |
|---|---|---|---|---|---|
| Q1（空 pop 返回元素） | pass/0 | pass/0 | fail(mismatch) | fail(mismatch) | 4 |
| Q2（LIFO 代替 FIFO） | pass/0 | pass/0 | fail(mismatch) | fail(mismatch) | 4 |
| Q3（重开保留旧元素） | pass/0 | pass/0 | fail(mismatch) | fail(mismatch) | 4 |
| Q4（peek 消费元素） | pass/0 | pass/0 | fail(mismatch) | fail(mismatch) | 4 |
| Q5（第二次 pop 不减计数） | pass/0 | pass/0 | fail(mismatch) | fail(mismatch) | 4 |
| Q6（close 不失效） | pass/0 | pass/0 | fail(mismatch) | fail(mismatch) | 4 |

汇总：24/24 判定正确；缺陷发现率 6/6=100%；正常误报率 0/12=0%；无工具崩溃、无超时、无残留进程。适配披露：每步新 adapter 进程 + 私有 state 文件状态桥（评审适配层，非产品能力）；脚本两处适配（COMMON 路径、timeout 10s 包装）已在第 4 节披露。
