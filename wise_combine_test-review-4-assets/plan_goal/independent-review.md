# wise_combine_test 第四轮评审报告 — Plan/Goal 方案（项目级证据）

- 评审对象：`/home/workspace_data/works/myprojects/wise_combine_test.plan_goal`（git 仓库，head `eb219c6`）
- 评审日期：2026-09-15；上一轮（review-3）基线：`e3f2e47`（2026-09-12）
- 评审标准：通用标准 v2.2 + AI 附件 v1.0 + 组合测试专项标准；AI 附件启用（项目为 AI Plan/Goal 编排交付）
- 评审方法：独立 subagent，未修改被评项目源代码；probe 与临时文件全部放在 `/tmp/pg_review4/`；review-3 冻结资产目录中的 `results/` 由脚本重写（该目录本就是评审产物目录）
- 证据优先级声明：本报告所有结论以可重复命令输出为准，标注 已验证/部分验证/未验证/明确不支持

---

## 1. 版本快照（已验证）

| 项 | 值 |
|---|---|
| git HEAD | `eb219c689b55dd3848990ce7371271fb51d5da5f` |
| git status（评审前后） | 均为空（干净；构建产物全部被 `.gitignore` 覆盖，含 `test/oracle/bin/`、`test/oracle/results/`） |
| 补救提交 | `git log --oneline e3f2e47..eb219c6` = 16 个提交，2026-09-12 19:11–20:38（约 87 分钟）：`636ede2 Fix plan_goal release blockers from review` → `1825463/d778e79/c25c253/d559ccf/c36db4c/4facccb/77cfbc5/b3cf442/aadc2b2/ef0d257/1f894fc/b673a86/54b1c1d/9b224b5/eb219c6` |
| 变更规模 | `git diff --stat e3f2e47..eb219c6 \| tail -3` = **35 files changed, 2171 insertions(+), 254 deletions(-)**（最大：`src/wise.cpp` +871/-、`test/test_wise.cpp` +425、`test/oracle/run_oracle.sh` 新增 68 行） |
| 编译器 | g++ (Ubuntu 9.4.0-1ubuntu1~20.04.2) 9.4.0；平台 Linux 5.15.0-139 |

---

## 2. review-3 遗留问题逐项核验

### a. 失败/超时/截断退出码恒 0（review-3 High #1）— **已修复（已验证）**

- 源码：`src/main.cpp:312-344` — `any_failure ? 4 : (truncated ? 5 : 0)`；ParseError→2（`:345-348`）、ModelError→3（`:349-351`）、一般异常→1（`:352-355`）。README「退出码」一节文档化了 0–5。
- 实测（命令均在 `/tmp/pg_review4/probes` 构造）：

| 场景 | 命令要点 | 退出码 |
|---|---|---:|
| 解析错误 | 残缺 object 块 | **2** |
| 模型错误（无初始状态） | | **3** |
| 缺参来源 | `function consume(token t)` | **3**（`model error: parameter consume.t has no source`） |
| 流程失败（direct 返回非零） | `always_fail` 库 | **4** |
| direct 超时（死循环函数，alarm） | 2 条 hang 流程各 10s，状态 `[timeout]` | **4** |
| adapter 超时 | 见 e | **4** |
| 生成截断 | `--max-flows 2` | **5** + `# warning: generation was truncated` |
| 日志/trace 写失败 | `--log-file /proc/version/xx`、`--trace /proc/version/xx` | **1**，错误信息明确（`src/wise.cpp:2300-2309`、`main.cpp:328-338`） |

- 24-run oracle：mutant 运行退出码全部为 **4**（见第 4 节），review-3 的"mutant 全部退出 0"已消除。

### b. direct/standalone 固定零参 ABI、参数只写注释（review-3 High #2）— **按"显式拒绝+文档化边界"修复（已验证，能力本身降级为边界）**

- direct：`src/wise.cpp:1699-1710` — 流程含带参函数或 `expect_output` 时该流程判 `failed`，detail 明确提示改用 adapter；实测带参模型 direct 运行 exit 4，无参流程照常执行。
- standalone：`src/wise.cpp:2206-2213` — 生成前抛 `runtime_error`；实测 exit 1，信息 `standalone mode does not support parameterized calls or expect_output; use --adapter`。不再出现"静默零参调用"或"参数只写注释"。
- adapter 是真实带参主路径（实测）：自建 adapter 中 `start` 步骤 `args={"h":"42"}`，值由 `init` 的 `returns.handle` 经 `parameter` 关系传入，7/7 流程通过。
- 边界诚实性核验（对照 git 历史）：`aadc2b2 Document current explicit boundaries`（20:23）晚于修复提交 `636ede2`（19:11）；`ai/proposal.md`/`ai/design.md`/README 同步改写为"direct/standalone 仅无参 C ABI，带参走 adapter"，与代码实际行为（拒绝而非静默错调）一致，`ai/defects.json` 同时保留 `D-101`（closed，修复=显式拒绝）和 `D-005`（open/future，类型化带参执行）——未把能力缺口伪装成已修复。
- 残留问题：direct 的报错文案 `use --adapter or --mode standalone`（`wise.cpp:1705-1706`）推荐 standalone，但 standalone 同样拒绝带参流程，指引错误（新发现 N5，Low）。跨模式语义不等价仍存在，但已是声明边界而非隐蔽缺陷。

### c. 字符串式 JSON 解析拒绝合法变体（review-3 High #3）— **已修复（已验证）**

- 源码：`src/wise.cpp:190-458` 全新递归下降 `JsonParser`（对象/数组/字符串/整数/bool/null、`\uXXXX` 转义、重复键拒绝、尾随内容拒绝、非整数拒绝），`parse_adapter_response`（`:460-517`）按字段类型校验。
- 实测 12 个协议变体（`/tmp/pg_review4/probes`，逐个独立 adapter）：

| 变体 | 结果 |
|---|---|
| 合法空格 `{ "protocol" : 1 , ... }` | 接受（exit 0）——review-3 被判 malformed 的场景 |
| 字段顺序重排（stdout 在前） | 接受 |
| 转义 `\n` `\u0041` | 接受 |
| 尾随内容 `} garbage` | 拒绝，`trailing content after JSON value` |
| protocol=2 / 1.5 | 拒绝（`protocol must be 1` / `non-integer numbers are not supported at byte 13`） |
| 缺 status/returns/stdout | 拒绝，各自带字段名 |
| 未知 status `weird` | 拒绝，`adapter reported weird` |
| 截断 JSON | 拒绝，`unexpected end of JSON at byte 23` |
| 重复键 | 拒绝，`duplicate object key at byte 44` |

### d. 缺少必需参数来源仍可通过、adapter 发空 args（review-3 High #4）— **已修复（已验证）**

- 源码：`src/wise.cpp:1188-1195` 模型期强制每个函数参数有来源；`:1099-1102` 多来源拒绝；`:1120-1141` 类型不匹配拒绝；运行期 `run_adapter`（`:1930-1948`）producer 缺失时流程判 failed。
- 实测：无来源→exit 3；`parameter start.x = init.h1` + `parameter start.x = "c"` → exit 3（`has multiple sources`）；类型不匹配 `long ← h1` → exit 3。

### e. max-flows 分别限额拼接翻倍；adapter 超时只杀单 PID（review-3 Medium #5/#6）— **均已修复（已验证）**

- 全局预算：`Generator::remaining_flows_`（`wise.cpp:1330-1331,1366,1435`）跨 state/function 生成器共享。实测 `--max-flows 1/2/3/5` 输出 total=1/2/3/5，全部 exit 5 + 截断警告；review-3 probe 曾得到 `--max-flows 1` 输出 2 条流程，现冻结脚本同 probe 输出 total=1。
- 进程组清理：`wise.cpp:1978,2001`（`setpgid` 双侧）、`:2052-2053`（超时 `kill(-pid, SIGTERM)`）、`:2090-2094`（100ms 后 `kill(-pid, SIGKILL)`）。实测：adapter fork 孙进程 `sleep 300` 后自身挂死 → 10s 超时、exit 4、`[timeout] adapter step timed out`，超时后 `ps` 确认 **无任何残留进程**。

### f. 报告缺 flow/config/seed/replay 元数据（review-3 Medium #7 + 新增声称）— **基本修复（已验证，余小项）**

- 实测 JSON 报告顶层字段：`seed, seed_set, version(plan_goal-1.0.0), model_digest(FNV-64), files[], total, passed, failed`；flow 级：`id(带序号防碰撞，wise.cpp:2323-2325), flow, status, exit_code, detail, expected, actual, bindings, steps[]`；step 级：`function, status, detail, expected, actual, args`。
- trace/replay：`--trace` 写出含元数据的 JSON；`--replay trace1.json` 恢复 7 条流程全部 passed，ID 一致（round-trip 已验证）；篡改 trace 中函数名为不存在值 → exit 4（失败暴露）。
- 余项：截断时 `# warning` 行追加在 JSON 之后导致输出非法 JSON（N3）；replay 不校验 trace 与当前模型 `model_digest` 一致（N4）；无环境（OS/编译器）与报告哈希字段。

### g. 函数生成器只有每函数一次的拓扑排列（review-3 组合缺口）— **已修复（已验证），但引入新语义问题 N1**

- 实测（多组 probe + 冻结 probe 重跑）：生成空流程（`1:`）、子集（`{a}`、`{b}`）、排列（`{a,b}`、`{b,a}` 受 order 过滤）、有界重复（`a->a`、`q_open->q_open->...`，`--max-function-repeats` 默认 2，设 1 后 `f->f` 消失）；count 约束 `count(a)==1` 同时过滤状态与函数流程；负向组合按 T5 生成。
- 单测 `test/test_wise.cpp:619-749` 断言空/子集/负向/重复与 count 六种算子；`make check` 通过。

### h. 状态流程未应用 mutex/count/value 约束（review-3）— **mutex/count/state 已修复（已验证）；value 约束有新缺陷 N2**

- `state_dfs`（`wise.cpp:1384-1390`）对终态路径统一检查 parameter/order/parallel/mutex/constraint/state 约束。
- 实测：`count(a)==1` 状态路径只剩 `a->c`；`mutex a b` 只剩 `b` 路径；`constraint state B` 只剩 `f` 路径（函数流程不受状态约束影响，符合语义）。
- value 约束：常量链不匹配时把**所有**流程（含不含该函数的流程与状态流程）全部排除，`total=0` 且无诊断——见 N2。

### i. `make check` 用 `-` 忽略 standalone 失败（review-3）— **已修复（已验证）**

- 当前 `Makefile:8-24`：`check` 依次跑单测、direct 示例、adapter 示例、standalone 生成+编译+**直接执行**（无 `-` 前缀）、`$(MAKE) oracle`；`test/oracle/run_oracle.sh:65-67` 任一 `FAIL` 行 → exit 1。实测 `make check`、`./ai/project-gate.sh` 均 exit 0（gate 还校验两份 JSON 合法且无 open high/blocker 缺陷，`ai/project-gate.sh:10-27`）。

---

## 3. 项目自身测试套件（已验证）

| 命令 | 退出码 | 结果 |
|---|---:|---|
| `make`（全量构建） | 0 | 无警告错误 |
| `make check` | 0 | 单测 `all tests passed`（约 250 条 `assert`，`-O2` 未定义 NDEBUG）+ 内嵌 oracle 24/24 PASS |
| `./test/out/test_wise` | 0 | `all tests passed` |
| `make asan` | 0 | `all tests passed`（2m20s，detect_leaks=0） |
| `make standalone` → `./build/wise_standalone` | 0/0 | 10 条流程全 passed |
| `make oracle`（`./test/oracle/run_oracle.sh` 直跑） | 0 | 24/24 PASS，总耗时 2.3s（预算 10s/run，远低于） |
| `make coverage` | 0 | `../src/wise.cpp` Lines **82.41%**、Branches executed **79.98%**、Taken once **50.57%**、Calls 69.53% — 与 `doc/coverage-report.md` 完全一致（数字可复现） |
| `./ai/project-gate.sh` | 0 | JSON 校验 + 无 open high + make check |

---

## 4. 统一 oracle 重跑（Q1–Q6 × clean/mutant × 2 = 24 run × 两种口径）

口径说明（与 review-3 冻结资产对比）：
- **项目内嵌口径**（`test/oracle/run_oracle.sh`）：Q1–Q6.ct、adapter.cpp、queue_sut.h 与 review-3 冻结资产逐字节/语义一致（diff 仅 include guard、注释与排版）；差异为增加 `--no-function-flows --max-flows 200`，且 verdict 判据加入退出码（clean 要求 rc=0，mutant 要求 rc≠0）。
- **review-3 冻结口径**（`wise_combine_test-review-3-assets/plan_goal/run_experiment.sh` 原样重跑）：state+function 流程、`--max-flows 100`、仅按 passed/failed 判 verdict。

**结果总表（每 run 的工具退出码单列）— 项目内嵌口径：**

| run | case | 变体 | 缺陷（mutant） | 重复 | 工具退出码 | passed/failed | oracle 判定 |
|---:|:--:|:--:|:--|:--:|---:|:--:|:--:|
| 1–2 | Q1 | clean | — | 1,2 | **0 / 0** | 1/0, 1/0 | PASS |
| 3–4 | Q1 | mutant | M1 empty pop returns item | 1,2 | **4 / 4** | 0/1, 0/1 | PASS |
| 5–6 | Q2 | clean | — | | **0 / 0** | 1/0, 1/0 | PASS |
| 7–8 | Q2 | mutant | M2 LIFO instead of FIFO | | **4 / 4** | 0/1, 0/1 | PASS |
| 9–10 | Q3 | clean | — | | **0 / 0** | 1/0, 1/0 | PASS |
| 11–12 | Q3 | mutant | M3 reopen keeps stale items | | **4 / 4** | 0/1, 0/1 | PASS |
| 13–14 | Q4 | clean | — | | **0 / 0** | 1/0, 1/0 | PASS |
| 15–16 | Q4 | mutant | M4 peek consumes item | | **4 / 4** | 0/1, 0/1 | PASS |
| 17–18 | Q5 | clean | — | | **0 / 0** | 1/0, 1/0 | PASS |
| 19–20 | Q5 | mutant | M5 second pop retains count | | **4 / 4** | 0/1, 0/1 | PASS |
| 21–22 | Q6 | clean | — | | **0 / 0** | 1/0, 1/0 | PASS |
| 23–24 | Q6 | mutant | M6 close does not invalidate | | **4 / 4** | 0/1, 0/1 | PASS |

统计：clean 12/12 通过（误报 **0/12**）；mutant 12/12 判失败且退出码全为 4；缺陷种类发现率 **6/6 = 100%**。

**review-3 冻结口径（同模型、含函数流程）结果表：**

| case | clean 退出码 | clean passed/failed | mutant 退出码 | mutant passed/failed | clean 判定 |
|:--:|:--:|:--:|:--:|:--:|:--:|
| Q1 | 0, 0 | 10/0, 10/0 | 4, 4 | 3/7, 3/7 | PASS, PASS |
| Q2 | 4, 4 | 45/55, 45/55 | 4, 4 | 32/68, 32/68 | **FAIL, FAIL（误报）** |
| Q3 | 4, 4 | 50/50, 50/50 | 4, 4 | 7/93, 7/93 | **FAIL, FAIL（误报）** |
| Q4 | 4, 4 | 32/68, 32/68 | 4, 4 | 25/75, 25/75 | **FAIL, FAIL（误报）** |
| Q5 | 4, 4 | 17/83, 17/83 | 4, 4 | 14/86, 14/86 | **FAIL, FAIL（误报）** |
| Q6 | 4, 4 | 24/14, 24/14 | 4, 4 | 9/29, 9/29 | **FAIL, FAIL（误报）** |

统计：缺陷发现率仍 **6/6**（mutant 全部 exit 4 且 failed>0），但 **正常误报率 = 10/12（83%）**——review-3 时该口径 clean 为 12/12 通过。成因定量分析（Q2–Q6 clean 共 270 条失败流程）：**262 条由重复调用触发**（如 `q_pop_first` 第二次返回 22，而 expect 是按函数名单一值 11 → `return mismatch`），8 条由跨流程状态残留/别名 expect 触发（如 Q6 `q_open->q_push1_after_close` 期望 -1 实得 0）。SUT 本身（MUTANT=0）行为正确，失败全部是工具判定语义造成。这说明：**问题 (g) 的修复（生成重复/子集流程）与"每函数单值 expect"判定模型不兼容，项目在内嵌 oracle 中以 `--no-function-flows` 规避，README 未对默认模式的该行为作出警告**（见 N1）。

执行判定 vs 自动发现（专项标准 §4 区分，不可互替）：上述 6/6 只证明"供应触发流程的执行判定链"（模型内 transition + 别名由评审资产提供）；本轮 probe 证明原生生成器**能自动产生**空/子集/重复/非自环重走/约束过滤流程（第 5 节），但未证明生成器会自动探索到 6 个 mutant 的精确触发序列（Q1–Q6 触发序列由模型 transition 供应）。

---

## 5. 生成 probe（原生生成器自动产生，非供应）

| probe | 模型/命令要点 | 结果（原生生成器行为） |
|---|---|---|
| 非自环重走 | review-3 `nonself_cycle.ct`（A→B→A 循环） | 状态路径 `to_b->to_a->to_b->...->finish` 多条有界重走；`max_state_visits=8` 截断（已验证） |
| 空流程 | 多模型 | `0:`/`1:` 空流程被生成并执行（空转 passed）（已验证） |
| 子集 | order 模型 | `{f}`、`{g}`、单函数流程生成（已验证） |
| 重复 | 默认 repeats=2 / 设 1 | `f->f` 等出现；`--max-function-repeats 1` 后消失（已验证） |
| 限额 | `--max-flows 1/2/3/5` | 全局预算生效，截断 exit 5（已验证） |
| count 约束 | `count(a)==1` 等 | 状态+函数流程同时过滤；六种算子单测覆盖（已验证） |
| mutex 约束 | `mutex a b` | 含双方的流程全排除（已验证） |
| state 约束 | `constraint state B` | 状态路径只剩终止于 B 的（已验证） |
| value 约束 | `value(q.x)=="bad"`（const） | **total=0 全局误杀**（缺陷 N2，已验证） |
| 参数传递 | `param.ct` + 自建 adapter | `args={"h":"42"}` 真实传递，producer→consumer 链路验证（已验证） |
| 多对象 | 两个 object 各自状态机 | 各自状态流程 + 函数流程跨对象交错（已验证） |

---

## 6. 新发现问题（本轮首次发现）

| 编号 | 严重度 | 专项+通用标签 | 问题 | 证据 |
|---|---|---|---|---|
| N1 | **Medium（偏高）** | `[CT-生成正确性] [CT-执行可靠性] [质量] [稳定性]` | 默认模式（函数流程开启）对带 expect 的有状态 SUT 系统性误报：重复调用与单值 expect 语义不兼容、跨流程 adapter 状态残留；clean Q2–Q6 全部 exit 4（冻结口径误报 10/12，262/270 失败由重复触发）；无"预期失败/负向标注"机制；项目自身 oracle 用 `--no-function-flows` 规避且 README 未警告默认模式该行为 | `results/Q2-clean-1.json` 等 10 份；`wise.cpp:2154-2180`（按函数名查 expect） |
| N2 | **Medium** | `[CT-生成正确性] [功能完整性] [可靠性]` | `constraint value(f.x) == "字面量"` 不满足时把所有流程（包括不含 f 的流程与状态流程）全部排除，`total=0` 且无任何诊断；对 producer 链接参数则静默跳过该约束 | `h_value4.ct` 实测 total=0；`wise.cpp:1673-1683`（未检查 `rel.func` 是否在流程中） |
| N3 | Low | `[CT-诊断复现] [可测试性]` | 截断警告 `# warning:` 追加在 JSON 报告之后，`--report json` 输出非法 JSON | `Q2-clean-1.json` 尾部；`main.cpp:341-343` |
| N4 | Low | `[CT-诊断复现] [可靠性]` | `--replay` 不校验 trace 的 `model_digest` 与当前模型一致，跨模型 replay 静默执行 | `other_model.ct` replay 实测 exit 4 无 mismatch 警告 |
| N5 | Low | `[质量] [可维护性]` | direct 报错文案推荐 `--mode standalone`，但 standalone 对带参流程同样拒绝 | `wise.cpp:1705` vs `:2209` |
| N6 | Low | `[CT-执行可靠性] [可扩展性]` | 超时硬编码 10s（`main.cpp:225`），无 CLI 选项 | 源码 |
| N7 | Low（信息） | `[CT-可观测性]` | 空流程计为 passed（空转通过）；`--seed` 仅记录不影响生成（生成完全确定），CLI 描述"复现实验种子"略有误导但无害 | probe 实测；`wise.cpp` 无 seed 消费点 |
| N8 | Low | `[CT-表达能力]` | value 约束仅支持常量链（DSL 文档已写"参数常量值"），producer 返回值场景静默跳过 | `h_value3.ct` |

无新的 High；review-3 的 4 项 High 全部确认修复。N1 若按"CI 对 clean SUT 默认误报"口径上调为 High，则发布门降为暂缓（本报告按 Medium 处理并列为发布条件）。

---

## 7. AI 附件五项评价（0-10）

核验对象：`ef0d257 Add AI and project management mechanisms`、`1f894fc Record AI management mechanism in usage log` 产出的 `ai/ai-usage-log.{md,json}`、`ai/defects.json`、`ai/project-gate.sh`、`ai/project-status.md`。

可追溯性核验（已验证）：JSON 台账 6 个条目引用的提交（636ede2/d559ccf/4facccb/77cfbc5/b3cf442/ef0d257/54b1c1d）全部存在于 git 历史；声称的验证命令 `make check`/`make asan`/`make coverage`/`project-gate.sh` 本轮全部独立复跑通过；覆盖率数字 82.41%/79.98% 独立复现一致。**不是模板文字**：条目内容与实际提交内容和边界决策一一对应（如"人工控制点：确认 direct/standalone 仅无参 C ABI"对应 aadc2b2/636ede2 的实际改动）。
关键缺口（项目自己如实登记为 D-006）：所有条目 `model_known:false, cost_known:false`——**模型名/版本、提示、token/费用、人工复核耗时均未采集**，因此不宣称 AI 净收益（这是诚实的，但按附件 §5 上限规则限制得分）。

| 维度 | 分 | 依据 |
|---|---:|---|
| AI 贡献效果 | **5.0** | 补救产出真实且全部验证通过，但无模型/版本/成本基线（附件规定此时 ≤5，取上限：结果可证、效率不可证） |
| AI 可延续资料 | **7.5** | proposal/design/task/decisions/checklists/defects/coverage 一致且我本轮实际用它定位了全部实现点；缺提示与调查原始记录 |
| AI 专用接口 | **2.0** | 无 RAG/索引/结构化查询接口；CLI/JSON 是普通工具接口（不自动计为 AI 接口） |
| AI 质量与安全 | **4.5** | 台账+门禁+ASan+可复现验证改善明显，但模型 provenance、提示审查、许可证/安全扫描缺失 |
| AI 成本与持续性 | **3.5** | 零成本/持续性情记录；优点是明确声明"未采集、不宣称净收益"，D-006 有 owner=human 跟踪 |
| **S_AI** | **4.5** | 五项平均 |

---

## 8. 七项软件分（独立重打）

| 维度 | review-3 | 本轮 | 一句理由 |
|---|---:|---:|---|
| ① 模型表达和契约 | 7.0 | **7.5** | 严格 JSON 协议+参数来源/多来源/类型校验+退出码契约文档化；但 DSL 仍有 value 约束误杀（N2/N8）、标量仅字符串、无模型版本兼容矩阵 |
| ② 组合生成正确性和覆盖 | 6.5 | **7.0** | 空/子集/重复/非自环/多对象全部实测生成、count/mutex/state 约束实测过滤、max-flows 全局生效；扣分：N1 默认模式误报语义、N2、无覆盖分母定义（branch-taken 仅 50.57%） |
| ③ 执行可靠性、隔离与安全 | 5.0 | **7.0** | 进程组清理实测无残留、双路超时、固定 env 的 execve、日志/trace 写失败显式 exit 1、退出码全谱实测；扣分：N1 误报、超时不可配（N6）、direct 无 stdout 捕获（已声明边界） |
| ④ 报告、诊断和复现 | 5.5 | **7.5** | seed/version/digest/files/expected/actual/steps/args 齐全、ID 防碰撞、trace/replay 往返与篡改实测、JSON 诊断到字节偏移；扣分：N3/N4、无环境与报告哈希 |
| ⑤ 工具自身测试完备性 | 7.0 | **7.5** | 单测+内嵌 24-run oracle 进 make check+ASan+覆盖率数字可复现+project-gate；扣分：模式×故障矩阵未完（D-007 open）、branch-taken 50.57% |
| ⑥ 设计、文档和后续维护 | 7.0 | **7.5** | 四处文档边界一致且与代码吻合（修复后书写）、D1–D11 决策、defects.json 机器可读；扣分：需求在补救期内改写（目标柱移动，虽已披露）、N5 文案不一致 |
| ⑦ 项目计划、风险和流程治理 | 6.0 | **7.0** | T32–T34 按期完成、缺陷台账含 severity/owner/deadline/commit、提交前门禁可执行；扣分：owner 几乎全为"AI"无真人分工、无工时/成本记录、D-001 分支覆盖 79.98% 未达 80（期限 09-19） |
| **平均 S_software** | 6.3 | **7.29 ≈ 7.3** | 主流程可靠、中等缺口（N1/N2）尚存，符合 7–8 锚点 |

`S_final = 0.85 × 7.29 + 0.15 × 4.5 = 6.87 ≈ 6.9`。AI 分不抵消软件问题；无未缓解 High，硬门槛未触发（若 N1 定级 High 则 ②③ 各再扣 1 分且发布门改暂缓）。

---

## 9. 阶段门结论

| 阶段 | 结论 | 一句依据 |
|---|---|---|
| 需求 | **通过** | R 编号需求+判定标准+边界（含补救期改写的 direct/standalone 边界）已冻结且与实现一致 |
| 设计 | **通过** | parser/validator/generator/runner/adapter/report 边界闭合，失败路径与退出码契约文档化并实测吻合 |
| 编码 | **通过** | 核心 parser/validator/generator/runner 回归+ASan 全绿，review-3 四项 High 全部修复并验证 |
| 测试 | **有条件通过** | 统一 oracle 6/6、误报 0/12（内嵌口径）、覆盖率可复现；条件：补 N1 默认模式语义警告或负向标注、修 N2、完成模式×故障矩阵（D-007） |
| 发布 | **有条件通过** | 干净构建/安装/门禁/回滚限制说明齐备且无未缓解 High；条件：修复 N1/N2（或文档化警告+规避指引）、D-001 分支覆盖 80%（09-19）后重跑 24-run；若协调方将 N1 定级 High 则本门降为**暂缓** |
| 维护 | **有条件通过** | 缺陷台账/门禁/checklist 闭环可用；条件：D-006 AI provenance 与工时/成本记录、每次 DSL/协议变更同步 proposal/design/task+回归 |

---

## 10. 证据覆盖率

分母（本轮适用且冻结的唯一检查项，47 项）：版本快照1；退出码契约7（失败/超时×2/截断/解析/模型/写失败）；direct/standalone 带参拒绝2；adapter 真实传参1；JSON 正例3+负例8；参数来源校验3（缺/多/类型）；max-flows1；进程组清理1；报告元数据1；trace/replay2（往返+篡改）；生成 probe8（空/子集/重复/非自环/限额/多对象/负向/count+mutex+state 合并为约束3）；单测/ASan/oracle/覆盖率/gate5；make check 失败传播1；AI 台账可追溯1；边界文档诚实性1。
- 有充分证据判为通过或失败：**45 项**（其中 3 项为"失败/缺陷"证据：N1、N2、value-producer 跳过——按标准计入覆盖不计入通过率）。
- 部分验证 2 项：direct 模式 `expect_output` 拒绝（代码+fixtures 证据，未单独实测）、T11 资源测量报告未复核。
- 不适用/明确非目标：随机策略、并发执行、RAG 接口、GUI。
- **证据覆盖率 = 45/47 ≈ 96%**（通过率另计：45 项中 42 项通过、3 项失败）。

### 附：评审纪律执行记录
- 未修改被评项目任何源码；`git status` 评审前后均为空。
- 所有 probe/临时文件在 `/tmp/pg_review4/`；冻结资产目录仅 `results/`（评审产物）被脚本重写。
- oracle 进程预算：内嵌口径全程 2.3s、冻结口径 12.0s（≪10s/进程）。
- 结束前 `ps` 复查：无本次评审残留子进程（检出的 node/Typora 进程属其它项目常驻）。

---

## 最终分数表

**七项软件分（S_software = 7.3）**

| ①模型表达和契约 | ②组合生成正确性和覆盖 | ③执行可靠性、隔离与安全 | ④报告、诊断和复现 | ⑤工具自身测试完备性 | ⑥设计、文档和后续维护 | ⑦项目计划、风险和流程治理 | 平均 |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 7.5 | 7.0 | 7.0 | 7.5 | 7.5 | 7.5 | 7.0 | **7.3** |

**AI 五项（S_AI = 4.5；S_final = 0.85×7.29+0.15×4.5 ≈ 6.9）**

| AI贡献效果 | AI可延续资料 | AI专用接口 | AI质量与安全 | AI成本与持续性 | S_AI |
|---:|---:|---:|---:|---:|---:|
| 5.0 | 7.5 | 2.0 | 4.5 | 3.5 | **4.5** |

**24-run 结果表（含每个 mutant 的工具退出码）**

| run | case | 变体 | 重复 | 内嵌口径退出码 | 冻结口径退出码 | failed 数（内嵌/冻结） | oracle 判定（内嵌） |
|---:|:--:|:--:|:--:|---:|---:|:--:|:--:|
| 1/2 | Q1 | clean | 1/2 | 0, 0 | 0, 0 | 0,0 / 0,0 | PASS |
| 3/4 | Q1 | mutant | 1/2 | **4, 4** | **4, 4** | 1,1 / 7,7 | PASS |
| 5/6 | Q2 | clean | 1/2 | 0, 0 | **4, 4** | 0,0 / 55,55 | PASS |
| 7/8 | Q2 | mutant | 1/2 | **4, 4** | **4, 4** | 1,1 / 68,68 | PASS |
| 9/10 | Q3 | clean | 1/2 | 0, 0 | **4, 4** | 0,0 / 50,50 | PASS |
| 11/12 | Q3 | mutant | 1/2 | **4, 4** | **4, 4** | 1,1 / 93,93 | PASS |
| 13/14 | Q4 | clean | 1/2 | 0, 0 | **4, 4** | 0,0 / 68,68 | PASS |
| 15/16 | Q4 | mutant | 1/2 | **4, 4** | **4, 4** | 1,1 / 75,75 | PASS |
| 17/18 | Q5 | clean | 1/2 | 0, 0 | **4, 4** | 0,0 / 83,83 | PASS |
| 19/20 | Q5 | mutant | 1/2 | **4, 4** | **4, 4** | 1,1 / 86,86 | PASS |
| 21/22 | Q6 | clean | 1/2 | 0, 0 | **4, 4** | 0,0 / 14,14 | PASS |
| 23/24 | Q6 | mutant | 1/2 | **4, 4** | **4, 4** | 1,1 / 29,29 | PASS |

缺陷发现率 6/6（两种口径一致）；正常误报率：内嵌口径 0/12，冻结口径 10/12（全部为 N1 判定语义问题，非 SUT 缺陷）；review-3 的"所有 mutant 工具退出码为 0"已修复为全部退出码 4。
