# OMX 方案第四轮独立评审报告（wise_combine_test，组合测试工具，C11）

- 评审人：第四轮 OMX 项目级独立评审 subagent
- 评审日期：2026-09-15（上一轮评审完成于 2026-09-12）
- 评审标准：《评价标准.md》v2.2（2026-09-12）、《评价标准-AI附件.md》v1.0、《评价标准-组合测试项目.md》
- 评审对象：`/home/workspace_data/works/myprojects/wise_combine_test.omx`（git 仓库，分支 `omx`）
- 能力分类口径：先按项目自身 README/需求/docs 将每项能力标为 **核心契约 / 已声明的可选扩展 / 明确非目标**；明确非目标不判为缺陷，但检查其与用户目标的一致性。
- 证据优先级：可重复命令 > 运行结果 > 源码 > 文档。所有实验命令与产物保存在 `/tmp/review4/`（冻结 oracle 复跑于 `/tmp/review4/frozen-omx/`，生成探针于 `/tmp/review4/probes/`）。
- 评审纪律声明：未修改被评项目源代码；构建/测试产生的变更仅限 `build/`、`bin/`（git 忽略）和 `coverage/` 三个被跟踪报告文件（`make clean` 阶段性删除后已用 `git checkout -- coverage/` 完整还原）；结束时 `git status --porcelain` 为空（0 行），HEAD 仍为 `1faa41e`。残留子进程已用 pkill 清理（验证 `ps` 无 sleep 20/lingering/grandchild 残留）。

---

## 1. 版本快照

| 项 | 值（已验证） |
|---|---|
| HEAD | `1faa41e26488165444b88fdb486b651c16fadf0a`（分支 `omx`，与 `origin/omx` 一致） |
| review-3 时 HEAD | `3985089`（21 个补救提交，2026-09-12 19:06 至 09-14 22:10） |
| diff 规模 | `180 files changed, 7502 insertions(+), 76 deletions(-)` |
| git status（评审前/后） | 前后均为干净工作区（本次评审中的临时删改已还原，见上） |
| 工具链 | gcc/cc 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.2)、GNU Make 4.2.1、Linux 5.15.0-139 x86_64 |

`git log --oneline 3985089..1faa41e`（21 项，节选关键项）：
```
1faa41e ultragoal: complete review blocker resolver
1e9bd8c docs: reconcile approved iter-18 release review
ea3c17b evidence: record iter-18 release candidate
4166916 fix: fail closed review gate edges      <- I18 证据绑定的源码候选
5dc6263 fix: resolve independent review blockers
911084b test: aggregate fork coverage profiles
41098fb test: add controlled queue oracle regression
19c17f0 fix: harden replay numbers and cron cleanup
ae264e7 fix: flush forked child gcov profiles
419fd8e fix: enforce duplicate declaration parser contract
df051a1 fix: enforce parser boundaries and cron runtime
c1047d7 docs: record OMX review improvement plan
```
发布候选为"源码 `4166916` + I18 证据"配对（`.omx/current-status.md`）；其后的 `d109f17..1faa41e` 均为文档/证据/治理提交。

---

## 2. review-3 遗留问题逐项核验

### 2a. sscanf 前缀解析接受尾随字符 —— **fixed（已验证）**

- 源码：新增 `parse_unsigned_strict()`（`src/wct.c:197-202`，`strtoull` + `end` 完整性 + ERANGE 检查）；`schema` 指令（`src/wct.c:326`）和 `contract` argc（`src/wct.c:332`）全部改走严格解析；CLI trace 侧新增 `parse_num_line()`（仅数字、拒绝 `+/-`、尾随内容，`tools/wct_cli.c:189-201`）和 `exact_single_value()`（`tools/wct_cli.c:218-226`），`seed/max_steps/max_flows/steps/exit` 等全部键行先经 `exact_single_value` 再严格数值解析（`tools/wct_cli.c:242-266`）。
- 实测（命令可重复，模型在 `/tmp/review4/models/`）：

| 输入 | 结果 | 退出码 |
|---|---|---|
| `schema 1x`（state/relation 两变体） | `error: line 1 column 1: unsupported schema` | 1 |
| `contract a 0x ...` | `error: line 4 column 1: invalid call contract` | 1 |
| trace 篡改 `seed 0x` / `max_steps 0x` / `steps 3x` / `exit 0x` | `error: invalid trace` | 1 |
| 同一 trace 未篡改 replay | `replay=PASS steps=3 digest=97135d47269d2afc` | 0 |

- 回归锁定：`tests/test_api.c:818-857`（`test_parser_strict_and_duplicate_declarations`，5 个负例含期望错误片段）；`tests/test_cli.sh:17-51` 含 `'seed 0x'`、`'process_exit 0x'`、`'exit 0x'` 等篡改矩阵。

### 2b. 重复顶层 state_graph/relation_graph 声明静默覆盖 —— **fixed（已验证）**

- 源码：`src/wct.c:316` 引入 `schema_seen/state_graph_seen/relation_graph_seen` 计数；`:326/:327/:330` 重复时报 `duplicate ...; first declared at line N` 并失败。
- 实测：`dup state_graph` → `error: line 3 column 1: duplicate state_graph; first declared at line 2`（exit 1）；`dup relation_graph`、`dup schema` 同样拒绝（exit 1）。回归在 `tests/test_api.c:818`。
- 附带验证：relation 层重复 `call a` ID 也被拒绝（`error: duplicate call`，exit 1，validator 层）。

### 2c. coverage 实测 18.87% 且 fork/child 归集未证明 —— **fixed/已验证归集策略**

- 机制（源码+脚本核验）：`make coverage` 导出 `WCT_COVERAGE_ROOT`；`src/wct.c:17-32` 的 `child_exit()` 在每次 fork 子退出前按子 PID 设置 `GCOV_PREFIX` 并调用 `__gcov_dump()`；`tools/merge-coverage.sh` 用 `gcov-tool merge` 将 11 个子目录的 profile 与父 profile 合并；`Makefile:90-94` 在必需的 `wct.gcda`/`wct_cli.gcda` fork profile 缺失时 fail-closed 退出。
- 本机复跑 `make coverage`（exit 0），`coverage/summary.txt`：

| 源文件 | Lines executed | Branches executed | Taken at least once |
|---|---:|---:|---:|
| `src/wct.c` | 78.55% (718) | 81.61% (1142) | 58.41% |
| `tools/wct_cli.c` | 92.01% (338) | 96.45% (620) | 67.10% |

Fork profiles aggregated: 22（与已提交的 `coverage/summary.txt`、I18 证据逐字一致，测量可复现）。
- 判定：归集策略已实现且有 fail-closed 证明；从 review-3 的 18.87% 提升到 78.55%/92.01%（行）。仍低于 80% 且 branch-taken 58.41%/67.10%——README:128-131 明确声明这是"measured baseline, not an 80% coverage claim"，属诚实披露的边界而非矛盾。

### 2d. release-readiness 待补项与 I15 PASS 状态冲突 —— **fixed（已验证）**

- `docs/release-readiness.md:8-28` 现明确区分：当前状态 **CONDITIONALLY PASS**（绑定 I18 @ `4166916`）与"Historical I15 Final Verification (2026-09-11)"小节（`:105-112`，声明 I15 仅为历史审计记录，不代表当前 HEAD）。
- 新增治理单页：`.omx/current-status.md`、`.omx/capability-matrix.md`（Supported/Unsupported 分类）、`.omx/release-blockers.md`（P0/P1/P2 → 关闭提交 → 回归锁定映射）、`docs/oracle-matrix.md`（回归↔语义映射）、`.omx/reviews/iter-18-final-review.md`（复核和解记录）。
- `evidence/iter-18/SHA256SUMS` 全部校验通过（`sha256sum -c`，RC=0）。
- 残余（记 Low，见第 6 节 L3）：`docs/improvement-plan.md:3` 头部仍写"当前版本仍按评审结论维持'发布暂缓'"（写于修复前的 09-12，未随修复刷新）；null-report/allocation-failure 注入回归仍列为待办（见 L2）。

---

## 3. 项目自身测试套件

入口：`Makefile`（`test`/`queue-oracle`/`sanitize`/`valgrind`/`measure`/`coverage`）。全部实跑：

| 命令 | 退出码 | 结果摘要 |
|---|---:|---|
| `make clean && make test` | 0 | `CLI smoke tests passed`；`API contract tests: PASS`（29 个 test_* 函数）；`DSL boundary fuzz tests passed` |
| `make sanitize`（ASan+UBSan 全套+队列 oracle+哨兵） | 0 | 84 matrix runs、12 clean、6/6 mutants、3 probes；`SANITIZER SENTINEL PASS: oob (exit=1)` / `leak (exit=134)`（预期故障分类单独判定） |
| `make valgrind` | 0 | `ERROR SUMMARY: 0 errors`（本机装有 Valgrind，非 SKIP）；队列 oracle 在 valgrind runner 下同样通过 |
| `make coverage` | 0 | 见 2c |
| `LC_ALL=C make measure OUT=...` | 0 | 3 次重复 + 中位数/极差表正常生成 |

测试脚本吞失败检查：`tests/test_cli.sh`、`test_queue_oracle.sh`、`test_fuzz.sh`、`sanitizer_sentinels.sh` 均为 `set -eu`，无 `|| true`、无 `exit 0` 兜底（`run_scheduled_ultragoal_test.sh:17,23` 的 `exit 0` 位于 mock 模式分支，属测试辅助，非产品断言）。`test_queue_oracle.sh:77-136` 不吞失败且反向锁死契约：精确 mutant→case 映射（各 2 次）、clean 必须 12/12、输出必须逐字节重复一致、probe 断言（cycle=1/subset=0/repeat=0）。

内嵌 oracle 与冻结 oracle 对比：内嵌 `tests/test_queue_oracle.sh`（84 行全矩阵）与冻结脚本（对角 24 核心 run）结论一致：clean 12/12、mutant 6/6、无误报、退出码仅 0/1。内嵌版 9 个模型与冻结评审模型**逐字节相同**（Q1–Q6 + 3 probes，`diff` 全部 IDENTICAL）；`tests/queue_sut.h` SHA256 `57847a37...` 与 common 冻结 SUT 相同；`tests/queue_harness.c` 仅 include 路径不同（AI 台账如实记录为 "created_from_review_asset_with_local_include_repair"）。

---

## 4. 统一 oracle 重跑（冻结资产，当前版本 1faa41e）

**适配披露（仅路径，无逻辑改动）**：`review-3-assets/omx/run_queue_experiment.sh` 与 `run_generation_probes.sh` 原引用不存在的 `wise_combine_test-review-2-assets/{omx,common}` 路径；本次仅将脚本内 `asset=`/`common=` 常量改为 `/tmp/review4/frozen-omx{,/common}`。模型文件、harness、编译命令（`cc -std=c11 -Wall -Wextra -Werror -DMUTANT=n`，链接当前版本 `src/wct.c`）、运行逻辑完全未改。冻结 harness 对 `wct_internal.h`/`wct_run_relation_in_process` 的依赖在当前版本仍成立，接口无破坏性变化。

**24 个核心 run 结果（clean/mutant 各 12）**：M0=clean（Q1–Q6×2）全部 `rc=0 failures=0 uncovered=0`、exit 0；匹配 mutant（M1→Q1 … M6→Q6，各×2）全部 `rc=-1 failures=1 error=call result assertion failed`、exit 1。M2（LIFO）另被 Q5 侦出（oracle-matrix 已文档化为额外检出，非误报）。每次重复输出逐字节一致。**缺陷发现率 6/6 = 100%；正常误报率 0/12；无崩溃/超时；工具退出码仅 0/1。** 与 review-3 结果完全一致（回归无退化）。完整 84 行矩阵 TSV 在 `/tmp/review4/frozen-omx/queue-results.tsv`。

**冻结生成 probes 复跑**：cycle_rc=1（`error=relation cycle`）、subset_rc=0（steps=2）、repeat_rc=0（steps=3）——与 review-3 逐字一致。

**执行判定 vs 自动发现的区分**（纪律要求）：以上 24 run 是**供应触发流程的执行判定**证据；Q1–Q6 流程由评审模型供应（现已内部化为回归 fixture），不证明生成器会自动发现这些缺陷流程。生成器自动发现能力见第 5 节。

---

## 5. 生成 probe（区分"原生自动产生"与"供应/适配"）

全部通过 CLI（`bin/wise-combine-test`）在新写模型上实测，命令与 trace 存于 `/tmp/review4/probes/`：

| Probe | 模型/命令要点 | 结果 | 判定 |
|---|---|---|---|
| 非自环状态边重走 | `idle --open--> ready --reset--> idle --close--> closed` | `steps=3 covered=3 uncovered=0`，exit 0 | **原生**：生成器自动经环回到 idle 后重走 `close` |
| 纯二环有界性 | `s0 --a--> s1 --b--> s0` | 默认 `steps=2 covered=2` 正常终止（每边计一次覆盖，不无限重走）；`--max-steps 1` → `uncovered=1` + exit 1（截断 fail-closed） | **原生**且有界 |
| 空流程 | `relation_graph empty`（零 call） | `steps=0 uncovered=0`，exit 0 | 可表达、稳定（退化空集） |
| 子集流程 | 两条独立链 `a→b`、`c→d` | 任一 flow 恒包含全部 4 个 call（`--max-flows 1` → steps=4；`=2` → steps=8，covered 去重为 4） | **非原生**：flow 是全 call 的拓扑排序/轮转，子集执行需单独声明子集模型（probe-subset）；与 capability-matrix "不做任意参数空间搜索" 的**明确非目标**一致 |
| 重复调用 | 重复 `call a` 被拒（`error: duplicate call`）；Q5 两连 pop 靠别名 `q_pop1/q_pop2` | 重复只能以唯一 ID 别名表达 | **DSL 契约成本**（文档已披露，review-3 同结论） |
| max steps/cases 限额 | `--max-steps 2`（relation） | `error: uncovered call`，`uncovered=2`，exit 1 | **原生**，截断非零退出、计数报告 |
| seed 可复现（relation） | `--seed 17`×2 / `--seed 48` / `--seed 0`，`--max-flows 2 --trace` | seed17 两次 trace 逐字节相同；seed48 顺序 `c,d,a,b` vs `a,b,c,d`；seed0 = `selection lexical-id-order` + flow2 声明序轮转 | **原生**：同 seed 可复现、异 seed 有意义差异、无 seed 策略显式记录在 trace `selection` 字段 |
| seed 可复现（state） | 分支图 `--seed 7`×2 / `--seed 21` | seed7 两次 trace 相同；seed21 分支顺序相反 | **原生**（API 侧另有 `test_seed_sampling_determinism`） |

结论：状态图可达边探索（含非自环重走、有界、截断报告）与确定性 seed 调度是**生成器原生能力**并有实证；子集/穷举组合与重复调用展开是**明确非目标/需别名适配**，未伪装成已有能力。oracle-matrix.md:97-104 明确"mutant 全发现只测执行器判错，不证明生成器完整性"，与实测一致。

---

## 6. 新发现问题（本轮新发现；上轮问题见第 2 节）

| 编号 | 严重度 | 专项/通用标签 | 问题 | 证据 |
|---|---|---|---|---|
| M1 | **Medium** | [CT-隔离安全][CT-执行可靠性] [稳定性][可靠性] | **超时只杀直接子进程，不清理进程组/后代**：`kill_reap()`（`src/wct.c:86`）仅 `kill(pid, SIGKILL)`，无 `setpgid`/`kill(-pgid)`。用公共 API `wct_run_relation` + `timeout_ms=500` 实测：工具正确判超时（`rc=-1 timed_out=1 signal=9`，立即返回），但场景子进程 fork 的孙进程（`lingering.sh`→`sleep 20`）在工具返回后仍存活，污染后续场景。专项标准 2.3 明确要求"超时按进程组清理子进程和后代"；README/api-contract 未声明"回调不得 fork 后代"的免责边界 | 复现程序 `/tmp/review4/probes/grandchild_probe.c`（外部 harness，未改项目源码），ps 快照显示孙进程残留 |
| L1 | Low | [CT-诊断复现] [可测试性] | CLI 失败摘要不含 expected/actual 值（仅错误类别+scenario+step，如 `error: call result type mismatch scenario=produce step=1`）；完整 expected/actual 仅在 C API `wct_report` 中。CLI 层独立复现复杂失败偏弱 | `tools/wct_cli.c` 打印路径 grep 无 expected/actual 输出；实测两例失败输出 |
| L2 | Low | [CT-测试完备性] [可测试性] | allocation-failure 注入与 null-report 指针回归仍缺失（项目在 `docs/improvement-plan.md` 阶段3、`docs/release-readiness.md:79` 如实列为待办；本轮 grep 证实测试套件中无对应注入） | `grep -rni "alloc.*fail" tests/` 仅命中文档 |
| L3 | Low | [可维护性] | `docs/improvement-plan.md:3` 头部"当前版本仍按评审结论维持'发布暂缓'"未随 09-14 修复刷新，与 `.omx/current-status.md` 的 CONDITIONALLY PASS 不同步（单一事实源指针已建立，影响有限） | 文件对照 |
| L4 | Low | [AI-可控性][AI-安全性] [稳定性] | `tools/run-scheduled-ultragoal.sh` 以 `--ask-for-approval never -s danger-full-access` 无人工批准点执行全权限 AI 代理（cron 触发；已做 one-shot 自移除与固定 Node 路径，9/13 ESM 失败有复盘）。无人值守 full-access 是治理风险，provenance 台账部分缓解 | 脚本 41-44 行 |

无 High 级新问题；review-3 唯一 High（严格解析）已修复并以回归锁定。

---

## 7. AI 附件五项评价（附件 v1.0，启用原因：项目由 AI 编排开发且宣称 AI 可持续维护）

**台账真实性核验（已验证，非模板文字）**：`.omx/ai/runs.jsonl` 3 条记录含模型（OpenAI `gpt-5.6-sol`）、UTC 时间戳、输入文件 SHA256、仓库基态（head/branch/clean/pending 文件清单）、输出及诚实动作描述（如 `created_from_review_asset_with_local_include_repair`）、`tool_boundary`（权限与显式 out-of-scope）、`adoption_status`、limitations（明示 "token and cost were not measured and are null, not zero"）。**哈希实测 3/3 完全吻合**：runs.jsonl 记录的 review-3 资产哈希与 `independent-review.md`（2a2314…）、`queue_harness.c`（279070…）、`queue_sut.h`（57847a…）实际 SHA256 一致——证明 09-14 的 AI 维护运行真实消费了上轮评审资产并驱动修复，形成"评审→计划→修复→证据"闭环。`.omx/ultragoal/ledger.jsonl`（33 事件）记录 goal 级 tokensUsed（748,628）/timeUsedSeconds（3,196s）及历史 2.13M/3.52M token 轮次与 `final_review_failed → REQUEST_CHANGES/BLOCK → 修复 → APPROVE` 过程。**不足**：单运行级 token/成本为 null；无 prompts/ 索引与 cost-report.tsv（改进方案建议未落地）；"independent review" 由同一 AI 平台会话执行、记录中无人工批准者身份；无模型升级离线重放（均已在 limitations 中如实声明）；L4 的 full-access 风险。

分项打分（0-10，依据见括号）：

| 维度 | 分 | 一句依据 |
|---|---:|---|
| AI 贡献效果 | 7.0 | 21 提交修复全部 review-3 P0/P1/P2 且经本评审独立复测为真（解析/重复声明/覆盖归集/oracle 内嵌），但缺人工/历史基线的效率与返工对比 |
| AI 可延续资料 | 8.0 | interviews/specs/plans/ultragoal/evidence + 哈希可验证的 runs.jsonl 实际支撑了后续 AI 任务复用（已证实），缺 prompt 索引与成本报表 |
| AI 专用接口 | 3.5 | 无 RAG/结构化查询接口；runs.jsonl+README 构成机器可读 provenance 模式（文档化 schema），按附件封顶规则不超过 5 |
| AI 质量与安全 | 6.0 | 模型/输入/输出/工具边界/采纳状态可追溯，自动门禁在 AI 改动后重跑通过；但无人工控制点身份、full-access 无批准闸、注入/凭证检查声明为范围外 |
| AI 成本与持续性 | 4.5 | ledger 有聚合 token/时长，单次计量为 null；单一供应商依赖、无离线/替代复现路径（已声明） |

**S_AI = (7.0+8.0+3.5+6.0+4.5)/5 = 5.8**

---

## 8. 专项七项软件评分（0-10，独立重打；锚点按通用标准 8.1）

| # | 维度 | 本轮分 | review-3 | 一句理由 |
|---|---|---:|---:|---|
| ① | 模型表达和契约 | **9.0** | 9.0 | 版本化 DSL/API 契约完整，严格数值/重复声明/行列号诊断已实现并回归锁定（实测），能力边界经 capability-matrix 冻结；guard/mutex/parallel 等为明确非目标 |
| ② | 组合生成正确性和覆盖 | **8.0** | 8.0 | 非自环重走、有界环、seed 采样、限额截断均为原生且实测通过；子集/穷举为声明边界且诚实声明"非生成完整性" |
| ③ | 执行可靠性、隔离与安全 | **7.5** | 8.0 | 隔离/事务回滚/超时/ASan/UBSan/Valgrind 全部复测通过，但新发现 M1（超时不清理进程组、孙进程存活）直接违反专项 2.3，较上轮下调 |
| ④ | 报告、诊断和复现 | **8.0** | 8.0 | trace 携 model/IR/metadata/step 四重摘要，数值字段严格解析后 replay 篡改全部拒收（实测 4 类）；截断/失败非零退出 fail-closed；CLI 缺 expected/actual（L1） |
| ⑤ | 工具自身测试完备性 | **8.5** | 8.0 | 队列 oracle 内嵌进 make test 并锁死 mutant 映射与逐字节重复、边界回归新增、fork 覆盖归集 78.55%/92.01% 可复现；仍缺 allocation-failure/null-report 注入（L2） |
| ⑥ | 设计、文档和后续维护 | **9.0** | 9.0 | capability-matrix/oracle-matrix/current-status/release-blockers/improvement-plan 交叉索引成链且与实测一致；仅 improvement-plan 头部过期（L3） |
| ⑦ | 项目计划、风险和流程治理 | **9.0** | 9.0 | 上轮全部 P0/P1/P2 两日内按"一修复一提交一证据"闭环，I16–I18 配对证据、复核和解与 SHA256SUMS 全部核验；单 AI 行为主体、无人工 owner 字段 |

**软件分 S_software = 59.0/7 = 8.4（8.43）**（review-3 为 8.4；③下调与⑤上调相抵，质量重心从"输入边界"移到"隔离完整性"）。
参考合成分：`S_final = 0.85×8.43 + 0.15×5.8 = 8.04`（AI 分不抵消软件问题）。

---

## 9. 阶段门结论（需求/设计/编码/测试/发布/维护）

| 阶段 | 结论 | 一句依据 |
|---|---|---|
| 需求 | **通过** | specs/prd/test-spec + 非目标冻结，capability-matrix 完成 Required/Unsupported 分类且与实测相符 |
| 设计 | **通过** | api-contract/oracle-matrix 将状态事务、关系 DAG、trace/隔离边界映射到具体回归，架构复核记录在案 |
| 编码 | **通过**（上轮有条件） | review-3 的 High/Medium（严格解析、重复声明）已修复并以 test_api/test_cli 回归锁定，无未缓解 High |
| 测试 | **有条件通过** | 统一 oracle 6/6 可复现、边界/篡改矩阵、fork 覆盖归集齐备；条件：补 allocation-failure/null-report 注入回归（L2），并对 M1 提供进程组清理证据 |
| 发布 | **有条件通过**（上轮暂缓） | I18 `AUTOMATED_GATES_PASS` + 双复核 APPROVE + 本轮独立复测全部通过；条件：修复 M1（进程组清理）后生成新证据迭代再发布；当前候选为 `4166916`+I18 配对，`4166916..1faa41e` 仅文档/治理提交可接受，但任何新源码提交必须刷新证据 |
| 维护 | **通过** | 评审→改进计划→修复→新证据→复核和解的维护闭环本身已被本轮实测验证为受控过程（哈希链完整） |

---

## 10. 证据覆盖率

分母清单（本轮冻结的适用检查项，34 项）：①git 快照 ②schema/contract 严格数值 ③trace 数值篡改 ④重复顶层声明 ⑤重复 call ID ⑥make test ⑦make sanitize ⑧make valgrind ⑨内嵌队列 oracle ⑩冻结 clean 12 run ⑪冻结 mutant 12 run ⑫冻结生成 probes ⑬内嵌 fixture 与冻结资产一致性 ⑭非自环重走 ⑮环有界/限额 ⑯空流程 ⑰子集语义 ⑱重复语义 ⑲relation seed 复现 ⑳state seed 复现 ㉑coverage 复跑+fork 归集 ㉒非法输入退出码 ㉓trace 写失败 ㉔超时判定与报告 ㉕进程组清理 ㉖CLI expected/actual ㉗iter-18 SHA256SUMS ㉘AI 哈希 provenance ㉙发布状态对齐 ㉚allocation-failure 注入存在性 ㉛make measure ㉜AI 人工控制点身份 ㉝提示注入/凭证防护 ㉞模型升级离线重放。

- 有充分证据判为通过或失败：**32/34 ≈ 94%**（①–㉙、㉚㉛㉜均为命令级实证，其中 ㉕㉖㉚㉜ 为**已证明的缺口/失败**，计入覆盖、不计入通过率）。
- 通过 26 / 证据化缺口 6（㉕ M1、㉖ L1、㉚ L2、㉜ 人工控制点、L3 文档过期、L4 full-access——后两项取自治理检查）。
- 无证据项（2）：㉝提示注入/凭证防护、㉞模型升级离线重放（项目声明范围外且本轮未测，维持未验证，不按通过计）。

---

## 附表一：七项软件分数表

| 维度 | ①模型表达和契约 | ②组合生成正确性和覆盖 | ③执行可靠性、隔离与安全 | ④报告、诊断和复现 | ⑤工具自身测试完备性 | ⑥设计、文档和后续维护 | ⑦项目计划、风险和流程治理 | 平均 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 本轮（review-4） | 9.0 | 8.0 | **7.5** | 8.0 | **8.5** | 9.0 | 9.0 | **8.4** |
| review-3 | 9.0 | 8.0 | 8.0 | 8.0 | 8.0 | 9.0 | 9.0 | 8.4 |

## 附表二：AI 附件五项分数表

| 维度 | 贡献效果 | 可延续资料 | 接口能力 | 质量与安全 | 成本与持续性 | S_AI |
|---|---:|---:|---:|---:|---:|---:|
| 分数 | 7.0 | 8.0 | 3.5 | 6.0 | 4.5 | **5.8** |
| review-3 | 5.0 | 8.0 | 3.0 | 5.0 | 4.0 | 5.0 |

（提升依据：runs.jsonl 哈希 3/3 实证吻合、维护闭环真实运转；接口能力仍无 AI 查询接口故按附件封顶规则低位。）

## 附表三：24-run 统一 oracle 结果表（当前 HEAD `1faa41e`，冻结脚本复跑）

| Run | Q1 | Q2 | Q3 | Q4 | Q5 | Q6 |
|---|---|---|---|---|---|---|
| M0 clean ×2 | PASS/PASS (exit 0, steps=2) | PASS/PASS (steps=4) | PASS/PASS (steps=5) | PASS/PASS (steps=4) | PASS/PASS (steps=6) | PASS/PASS (steps=3) |
| M1 空 pop 错值 | **FAIL/FAIL** (exit 1) | PASS | PASS | PASS | PASS | PASS |
| M2 LIFO | PASS | **FAIL/FAIL** | PASS | PASS | FAIL/FAIL† | PASS |
| M3 reopen 残留 | PASS | PASS | **FAIL/FAIL** | PASS | PASS | PASS |
| M4 peek 消费 | PASS | PASS | PASS | **FAIL/FAIL** | PASS | PASS |
| M5 二次 pop 计数不减 | PASS | PASS | PASS | PASS | **FAIL/FAIL** | PASS |
| M6 close 失效 | PASS | PASS | PASS | PASS | PASS | **FAIL/FAIL** |

† M2 被 Q5 额外检出（oracle-matrix 文档化的额外检出，非误报）。
**缺陷发现率 = 6/6 = 100%（12/12 mutant run 判失败）；正常误报率 = 0/12；全部 run 输出逐字节可重复；工具退出码仅 0/1；无崩溃/超时。** 与 review-3 结果及项目内嵌 84-run 回归三方一致。

---

**总体结论**：OMX 在两日内以 21 个提交关闭了 review-3 的全部遗留问题（严格解析 High、重复声明 Medium、fork 覆盖归集、状态冲突治理），所有修复经本轮独立命令级复测确认真实且已回归锁定；冻结 oracle 与生成 probes 在当前版本完整复现。本轮新发现 1 个 Medium（超时不清理进程组/后代，M1）与 4 个 Low。软件分 8.4（③降至 7.5、⑤升至 8.5），AI 分 5.8（哈希验证的真实 provenance 是四个方案中最扎实的一档）；发布阶段门从"暂缓"上调为"有条件通过"，条件为修复 M1 并补 allocation-failure/null-report 注入后刷新证据迭代。
