# wise_combine_test.manual 第四轮评审报告（Manual 方案）

- 评审日期：2026-09-15；评审人：独立软件评审 subagent（第四轮，Manual 专项）
- 评审依据：《评价标准.md》v2.2、《评价标准-AI附件.md》v1.0、《评价标准-组合测试项目.md》、review-3 报告及冻结资产（`wise_combine_test-review-3-assets/`，common 为冻结有状态队列 SUT 与 Q1–Q6 用例）
- 评审方式：全部结论基于本轮可重复命令实测；oracle 进程预算 10 秒/个已逐项计时；结束前已确认无残留子进程
- git status 变化：评审前后被评项目工作区均干净（`git status --short` 为空）；构建/测试产物均位于 `build/`（gitignore）与 `test/oracle/*.tsv`（gitignore），无源码改动。sabotage 试验（第 3 节）临时改写 `test/run.sh` 后已还原并经 git status 确认

---

## 1. 版本快照

| 项 | 值 |
|---|---|
| git HEAD | `7eb9ce108f95b734060118b3317fc07bcfdc82c7` |
| git status | 干净（无未提交/未跟踪非忽略文件） |
| `git log --oneline 96d639a..7eb9ce1` | 11 个提交：6bd117c 修复 guard/CLI/执行隔离并补齐统一 oracle；85ccbaf 拒绝重复声明并补充模型校验测试；37f97c2 补充 JSON 报告结构说明和资料索引；533a487 忽略 oracle 生成结果文件；5307439 增加 CI 回归目标并记录 AI 续开发演练；c19ba9a 新增能力状态与边界矩阵；c3c0a3a JSON 报告增加稳定 case_id；5b70c8c 补齐 examples 目录说明；b3e0250 实现函数状态变量与生成探测；748ddb4 建立 AI 成本基线与项目治理流程；7eb9ce1 补齐复评台账与复评说明 |
| `git diff --stat 96d639a..7eb9ce1 \| tail -3` | `test/run.sh +104`、`test/slow_sut.c +5`、`46 files changed, 2081 insertions(+), 230 deletions(-)` |
| 提交时间分布 | 全部 11 个提交集中在 2026-09-12 19:36–20:53（review-3 报告定稿 18:26 之后 77 分钟内） |
| 编译器 | gcc/g++ 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.2)、cmake 3.16.3、GNU Make 4.2.1、Java（ANTLR 4.13.2 随仓库） |

---

## 2. review-3 遗留问题逐项核验

| # | review-3 问题 | 结论 | 证据（命令/位置） |
|---|---|---|---|
| a | README 宣称 `--algorithm dfs` 但显式输入报 unknown algorithm | **fixed（已验证）** | `./build/wise_combine_test doc/examples/file-functions.dsl --algorithm dfs` → exit 0 正常生成 21 序列；`--algorithm bogus` → `unknown algorithm: bogus` exit 2；回归断言 `test/run.sh:91-92` |
| b | BFS 忽略 `--max-cases`、tour 忽略 `--max-length` | **fixed（已验证）** | BFS `--max-cases 5/2` → 序列数 5/2；DFS 3→3；random 4→4；tour `--max-length 2` → 路径截断并输出 `tour truncated: true` + `uncovered_transitions`；tour 单路径性质下 max-cases 无意义（实测 paths:1 一致）。实现：`src/sequence_generator.cpp:92,130`、`src/state_machine_path_generator.cpp:69-71,114,133,151,190-194,237` |
| c | 状态路径生成不求 guard，可能生成不可执行路径 | **fixed（已验证）** | `src/state_machine_path_generator.cpp:73-89`（`canFire` 调 `guard::evalGuard`）；实测 guard.dsl：`--guard result=OK` 只生成 `B -ok-> C` 路径，`--guard result=FAIL` 只生成 `B -ok-> D`，未绑定 → 跳过并输出 `skipped_guards: 2`（JSON 同样输出）；`--events` 执行未绑定 → `guard variable 'result' is unbound` exit 1；三处共用 `src/guard.cpp:114-230` 求值器 |
| d | 生成 harness 只精确匹配叶状态，nested `power_on,power_off` 普通执行通过、harness 失败 | **fixed（已验证）** | 生成的 harness 中父状态转换 `ON -> OFF` 展开为 `(strcmp(current,"ON_IDLE")==0 \|\| strcmp(current,"ON_WORKING")==0)`（`src/harness_generator.cpp:659-681` fireableLeaves）；实测 `--events power_on,start,power_off --harness` 生成代码 g++ 编译运行 `ALL PASS` exit 0；回归 `test/run.sh:113-116` |
| e | 未知 guard 变量静默当 true；重复声明被 map 覆盖 | **fixed（部分验证，留 1 个 Low 残留）** | 未绑定 guard：见 (c)，不再静默成立。重复声明：函数 DSL 的 type/value/resource/class/setup/function 重复 → `duplicate xxx` 语义错误 exit 1（`src/function_model_builder.cpp:101,112,134,236,256,287,302`）；状态机 state/state block/class/action 重复 → exit 1（`src/state_machine_builder.cpp:57,111,124,152`），实测复现。**残留**：完全相同的重复 transition（同 from/event/to）仍被静默接受（实测 dup_sm2.dsl 2 条 transition、exit 0）→ 新问题 N4 |
| f | `test/run.sh` 的 `\|\| true` 削弱失败传播 | **fixed（已验证）** | 全部三个脚本（run.sh、oracle/run_oracle.sh、oracle/generation_probes.sh）grep `\|\| true`/吞错模式零命中；预期失败均用 `set +e; rc=$?; set -e` + 显式 `[ "$rc" -ne 0 ]` 断言（如 `test/run.sh:39-44,49-54,83-89,161-167,171-177`），符合通用标准 §8.2 允许的"预期失败显式核对"模式；**sabotage 试验**：把一处期望值改成 `paths: 999` 后 `bash test/run.sh` exit 1（失败正确传播），随后已还原 |
| g | 统一 oracle 未在预算内完成（生成器先枚举大量额外序列、精确触发脚本超时） | **fixed（已验证，本轮重点）** | 项目内嵌 `test/oracle/run_oracle.sh` 实测 24/24 PASS + native-q2 6/6 PASS，总耗时 2.7s（约 90ms/run，远低于 10s 预算）；独立脱离脚本复跑 Q5/Q1/Q3 clean+mutant 全部吻合，mutant 在真实不匹配点失败（见第 4 节）。review-3 冻结脚本（生成→replay 索引）在隔离副本重放：1.0s 内完成 q1/q2（clean PASS、mutant 在真实 mismatch 失败），但 q3/q5 因该脚本自带 `--max-cases 100` 上限在索引 196/440 处查不到目标而 StopIteration——这是该冻结脚本"固定 cap + 固定枚举顺序"的自身限制，生成速度瓶颈已消除（q5 全枚举 465 序列 <0.2s） |

补充：review-3 另一 Medium"模型无法表达冻结队列 count/value 关系"→ **fixed（已验证）**：新增 `var`/`list`/`update` 与 `len()`/`front()`（`doc/dsl.md:57-97`），`test/oracle/native-q2.dsl` 用单一 `q_pop` 函数 + 影子列表表达 FIFO oracle，无别名函数；实测 mutant-2 报 `expected:"result==front(items)", actual:"22"`。该能力在能力矩阵中如实标为 Advertised，未夸大。

---

## 3. 项目自身测试套件

| 命令 | 结果 | 耗时 |
|---|---|---|
| `make test`（= `bash test/run.sh`） | 输出 `PASS`，exit 0 | 1.8s |
| `make oracle`（run_oracle.sh + generation_probes.sh） | `unified oracle passed` + `generation probes passed`，exit 0 | 2.7s + 0.19s |
| `make ci`（clean → all → test → oracle） | 全绿，exit 0（137 个编译单元全量重建） | 后台完成 |

测试脚本吞失败检查：无 `|| true`、无重定向丢弃关键输出；预期失败（JSON failed、guard 未绑定、超时、harness-json 失败、重复声明拒绝、CLI 非法值）全部显式断言非零退出码与输出内容。sabotage 试验证明任一断言失败会以 exit 1 终止整个套件。测试面覆盖：固定计数回归（21/5/101 序列等）、guard 三路径一致、嵌套/历史/并发执行、重复声明、CLI 严格校验（`--max-length -1`、`1x` 拒绝）、BFS/tour 限额、harness 编译运行（C、C++ 类、dylib）、超时（slow.dsl `--timeout 1` → `TIMEOUT`）、supplied-trigger、独立 Python 参考实现对照（`test/reference_check.py`）。

---

## 4. 统一 oracle 重跑（Q1–Q6，24-run）

**口径对比**：项目内嵌 `test/oracle/` 与 review-3 冻结资产逐文件比对——`queue_adapter.c` 字节一致；`queue_sut.h` 仅格式差异并新增 `q_pushv`（供 native-q2 用，不改变既有 6 个 mutant 的语义）；q1–q6.dsl 仅缩进差异、语义一致。**协议差异**：内嵌脚本用新 CLI 入口 `--sequence`（精确供应触发，跳过枚举——正是 review-3 建议的补救方向），每 run 仍 `timeout 10s`；另加 native-q2（0/2/5 三个 mutant × 2 轮）演示原生状态变量表达。供应触发执行判定与生成器自动发现分别由 `run_oracle.sh` 和 `generation_probes.sh` 承担，未互相替代。

**24-run 结果（本轮实测两遍：脚本自带 2 轮 + 独立抽检）**：

| 指标 | 结果 |
|---|---|
| clean 对照 | 12/12 PASS（exit 0，`ALL PASS`） |
| mutant 判错 | 12/12 PASS（exit 1，全部在真实返回值不匹配处失败） |
| 缺陷种类发现率 | **6/6 = 100%** |
| 正常用例误报率 | **0/12** |
| 单 case 耗时 | 生成 <10ms、编译+执行 ≈90ms，全部远低于 10s 预算；无超时、无崩溃、无残留进程 |
| native-q2（追加） | 6/6 PASS（mutant-2/5 被原生 count/value oracle 捕获） |

**独立抽检**（脱离项目脚本、`--harness-json` 直接编译运行，防脚本造假）：
- Q5 MUTANT=5：`{"kind":"failure","seq":0,"step":"q_size_empty","expected":"result==0","actual":"1"}`，exit 1
- Q1 MUTANT=1：`step q_pop_empty, expected "result<0", actual "7"`，exit 1
- Q3 MUTANT=3：`step q_size_nonempty, expected "result==0", actual "1"`，exit 1
- 断言链真实：SUT/adapter 无断言（冻结 SUT 原文），断言由 DSL `success:`/`update:` 编译进 harness，实际值来自被测函数返回值；SUT 崩溃（SIGSEGV）实测 → harness `FAILED 1` exit 1。

结论：review-3 的"未验证"状态已解除，本轮记 **已验证 6/6、0 误报**。

---

## 5. 生成 probe（自动发现与生成语义）

| probe | 所测对象 | 结果 | 证据 |
|---|---|---|---|
| 目标序列自动发现 | 生成器（非供应触发） | **PASS（带 cap 保留意见）** | `generation_probes.sh` 实测：6/6 目标序列由 DFS 枚举自动产生（q1 idx2/3、q2 96/101、q3 196/252、q4 26/31、q5 440/465、q6 7/8），总耗时 0.19s。**保留意见**：该 probe 未套用冻结协议的 `--max-cases 100` 上限；实测加 cap 后 q5（idx 440）、q3（idx 196）不可达，即冻结预算下自动发现为 4/6——发现能力受枚举顺序影响，需如实披露 |
| 非自环状态边重走 | state cycle | PASS | nested.dsl max-length 4：4 条路径两次经过 `OFF -power_on-> ON` |
| 子集 | subset | PASS | native-q2：504 序列覆盖 8 种函数子集 |
| 重复调用（非自环/自环） | repeat | PASS | q_push 两次 21 条、q_pop 两次 163 条（native 模型）；SM 非自环重走见上 |
| 空流程 | subset/empty | **明确不支持且未在矩阵分类** | 生成最小长度恒为 1；`--sequence ';'` → `empty --sequence` exit 1。能力矩阵未单列此项（N6，Low） |
| 限额 | limits | PASS | 见第 2 节 (b)；tour 截断有 `truncated/uncovered_transitions` 报告 |
| guard 过滤 | 约束过滤 | PASS | 见第 2 节 (c)；skipped_guards 进文本与 JSON |
| 参数传递 | 执行模式一致性 | PASS | harness 实际展开 `q_pushv(11)`/`q_pushv(22)` 字面传参、影子列表 `shadow_Queue_items` 更新、断言 `r3==shadow_Queue_items[0]`；dylib/直接链接/`--sequence` 三模式均有回归 |
| seed/确定性 | determinism | PASS | 同 seed 两次输出 md5 相同、异 seed 不同 |
| 多对象 | 多资源实例 | PASS | file-functions max-length 4：3 条序列同时操作句柄 0 和 1（`fclose(0);fclose(1)` 及反序） |
| 崩溃隔离 | 执行器 | PASS | SIGSEGV SUT → 子进程死亡 → 父进程 `FAILED 1` exit 1（fork/setpgid/`kill(-pid,SIGKILL)`，`src/harness_generator.cpp:247-279`） |

---

## 6. 新发现问题（本轮）

| 编号 | 严重度 | 标签 | 问题 | 证据 |
|---|---|---|---|---|
| N1 | **Medium** | `[CT-表达能力] [CT-生成正确性] [可靠性]` | `--sequence` 供应序列不校验资源参数来源：`q2.dsl --sequence 'q_pop_two(0)'`（无前置 q_open 产生句柄）被接受并生成引用未声明 `h0` 的不可编译 harness，而非在模型/序列阶段按专项标准 §2.1"来源缺失必须拒绝"给出诊断（失败是响亮的编译错误，不会误判通过，故不升 High） | `src/main.cpp:156-174`（仅查参数个数）；实测 gcc 报 `error: 'h0' undeclared` |
| N2 | **Medium** | `[CT-执行可靠性] [CT-隔离安全]` | 生成 harness 子进程 stdout/stderr 无输出上限、SIGPIPE 未处理（专项标准 §2.3 要求输出上限有界）；能力矩阵未对该边界做任何分类声明 | `src/harness_generator.cpp:247-279`（run_test 无输出限流）；超时/进程组/崩溃已实现，唯输出上限缺失 |
| N3 | **Medium** | `[CT-诊断复现] [可维护性]` | 执行/隔离模型、`--sequence`、状态变量三次边界级变更未按项目自身 `doc/governance.md` 变更影响规则同步 `ai/design.md`（自建立后未改）和新增 ADR（仅 ADR-014 追加一行），设计决策只散落在 task.md/README | `git log -- ai/design.md`（仅 d581540）；ADR 止于 026 |
| N4 | Low | `[CT-表达能力]` | 完全相同的重复 transition 声明被静默接受（虚增 transitions 计数与覆盖分母）；其余重复声明均已拒绝 | 见第 2 节 (e) |
| N5 | Low | `[CT-表达能力] [质量]` | `--sequence` 传入状态机模型时被静默忽略（正常枚举路径执行、exit 0），与其"不再静默"的 CLI 契约基调不一致（README 已注明仅函数模型，属诊断缺失而非功能缺失） | 实测 `connection.dsl --sequence 'connect'` → 枚举输出 exit 0 |
| N6 | Low | `[CT-表达能力] [可维护性]` | 空流程能力不支持但未在能力矩阵分类；`--max-cases 0`=不限、random 下 0→默认 20 次尝试的语义差异未在 README 说明 | 见第 5 节；`src/state_machine_path_generator.cpp:144` |
| N7 | Low | `[CT-诊断复现] [效率]` | 内嵌 oracle 用 `mktemp -d` + `rm -rf` 删除全部中间产物，仅留 results.tsv，单 run 日志/harness 不可事后审计（冻结协议要求产物留存；本轮以独立复跑补齐证据） | `test/oracle/run_oracle.sh:31,56` |
| N8 | Low | `[AI-可控性] [可维护性]` | `ai/index.md` 各资料"适用 commit"列仍写"待提交"（已提交）；`ai/ai-security-checklist.md` 全部勾选框未对已发生的演练实际勾选 | 两文件原文 |

能力矩阵诚信度检查（是否有未修缺陷伪装成非目标）：**未发现伪装**。矩阵中 Required 项（guard 统一语义、算法、限额、harness、`--sequence`、`--timeout` 隔离）全部实测通过；Advertised 项（嵌套/历史/并发、状态变量）有对应回归且通过；Unsupported 项（经典 t-way 参数组合、GUI、分布式）与 proposal 一致且不与用户目标冲突。N2（输出上限）属于矩阵漏列的边界，而非伪装。

---

## 7. AI 附件五项评价（0–10）

启用原因：项目由 AI（codex/GPT-5）完成 review-3 后的全部维护改造，且 `ai/` 下沉淀了供 AI 续用的资料。

1. **AI 贡献效果：5.0**。改造真实且高质量：11 个提交（19:36–20:53 共 77 分钟、46 文件 +2081 行）逐项对应 review-3 的 High/Medium，全部经本轮实测确认修复（guard 统一、隔离、oracle 6/6、状态变量）；`make ci` 可复现。但模型版本"未记录"、token/费用/人工审查时长全部未记录、无同任务人工基线——按附件 §5"没有模型/版本/人工控制/质量证据时不得超过 5 分"，版本与成本证据缺失，封顶 5 分。
2. **AI 可延续资料：7.0**。`ai/index.md`（DOC-001~014 带 ID/日期/路径）、ADR、capabilities、report-schema、review-notes、task 台账齐全且互相可追；"AI 续开发演练"即本次 review-3 驱动改造本身，产出可复现（本轮全部重跑验证），非模板文字——AI-USAGE 四行记录均对应真实提交与回归证据。扣分：资料版本绑定列陈旧（N8）、无结构化检索接口。
3. **AI 专用接口：2.0**。无 RAG/代码索引/结构化 AI 查询接口；CLI+JSON 属普通产品接口，按附件规则不计入。产品可在无 AI 环境完整构建回归（离线性好，但这属于持续性而非接口能力）。
4. **AI 质量与安全：4.0**。AI 产出经人工批准留痕（"用户 2026-09-12 指示提交"）、git 可回滚、`make ci` 回归验证；`.agents/auth.json` 经 `git log --all` 确认从未入库；harness 现具备子进程/超时/进程组隔离。扣分：安全检查表是未勾选的模板（N8）、模型版本未记录、提示原文未存档（只有摘要）。
5. **AI 成本与持续性：3.0**。`ai/AI-COST.md` 诚实标注基线"未建立/未记录"（未虚报），离线替代路径（人工 + `ai/index.md` + `make ci`）和供应商切换预案成文；但除"未记录"外无任何真实数据。

**S_AI = (5.0+7.0+2.0+4.0+3.0)/5 = 4.2**

---

## 8. 七项软件分（独立重打）

| 维度 | review-3 | 本轮 | 一句理由 |
|---|---:|---:|---|
| ① 模型表达和契约 | 6.0 | **7.5** | guard 三路径统一、var/list 原生表达 count/value、重复声明拒绝、CLI 严格校验；但 `--sequence` 不校验资源来源（N1）和重复 transition（N4）留有契约缺口 |
| ② 组合生成正确性和覆盖 | 5.0 | **7.0** | 四算法限额/truncation/skipped_guards/determinism/重走/子集/重复/多对象全部实测通过；自动发现 6/6 但冻结 100-case 预算下仅 4/6（枚举顺序依赖） |
| ③ 执行可靠性、隔离与安全 | 3.0 | **7.0** | 子进程+进程组+超时+崩溃非零退出全部实测（slow/SIGSEGV/oracle 30 run）；缺输出上限与 SIGPIPE 处理（N2） |
| ④ 报告、诊断和复现 | 4.0 | **6.5** | version/model_hash/seed/case_ids（实测稳定）/expected/actual 齐全且有 schema 文档；失败记录仍缺完整参数、环境与可复制重放命令，oracle 中间产物不留存（N7） |
| ⑤ 工具自身测试完备性 | 3.0 | **7.0** | make test/oracle/ci 三层回归 + 故障注入（guard/超时/崩溃/重复/负例）+ 独立参考实现对照 + sabotage 验证失败传播；缺 fuzz/sanitizer/输出上限/报告写失败类测试，覆盖率未量化 |
| ⑥ 设计、文档和后续维护 | 6.0 | **6.5** | capabilities/report-schema/governance/dsl 文档同步良好、矩阵诚信；但隔离模型与两项新入口未按自身治理规则同步 design/ADR（N3） |
| ⑦ 项目计划、风险和流程治理 | 4.0 | **5.5** | 治理框架、阶段门、缺陷闭环（review-3 项全部修复留证）、复评台账成文且执行了一次真实闭环；owner/deadline 仍为模板、工期/成本无数据、AI 成本基线空 |
| **平均** | 4.4 | **6.7** | |

硬门槛核对：无未缓解 High（review-3 两个 High 均已修复并回归）；核心需求通过；可靠性关键场景（超时/崩溃/mutant 判错）已验证；oracle 独立冻结且证据覆盖率 94%（第 10 节）≥60%——无分数封顶适用。

---

## 9. 阶段门结论

| 阶段 | 结论 | 一句依据 |
|---|---|---|
| 需求 | **通过** | proposal/DSL 文档/能力矩阵把目标、范围、非目标、验收 oracle 冻结且分类与实测一致 |
| 设计 | **有条件通过** | 生成/执行/报告边界已闭合并实现；条件：按 governance 规则把隔离模型、`--sequence`、状态变量补入 design.md/ADR（N3） |
| 编码 | **通过** | parser/validator/generator/runner 回归全绿，review-3 High 全部修复，无未缓解 High |
| 测试 | **有条件通过** | make test/oracle/ci + 故障注入 + 24-run oracle 6/6 + 生成 probe 齐全；条件：补 `--sequence` 资源来源校验（N1）、输出上限（N2）回归后再进发布 |
| 发布 | **有条件通过**（review-3 为不通过） | `make ci` 作为统一发布门存在且实测全绿、安装/回滚路径成文；条件：关闭 N1/N2、oracle 产物留存（N7）、填充 owner/deadline |
| 维护 | **有条件通过** | 变更影响规则 + 复评台账 + 一次真实 AI 续开发闭环已验证；条件：AI 成本/token 回填、index 版本绑定修正（N8） |

---

## 10. 证据覆盖率

分母（适用检查项 33 项，评审前依任务书冻结）：a/b/c/d/e/f/g 七项遗留（拆为 a、b、c、d、e-未绑定、e-重复声明、g-oracle 共 7）；make test、make ci、脚本吞失败、CLI 数值校验、--sequence 错误处理（未知函数/参数个数）、--sequence 资源来源、--sequence 作用域（SM）、seed 确定性、非自环重走、子集、空流程、重复、多对象、限额、guard 过滤、参数传递、崩溃隔离、超时隔离、case_id 稳定、JSON 字段、能力矩阵诚信、凭证入库、AI 演练真实性、AI 成本基线、残留进程（共 26）。

- 有充分证据判为通过或失败：**31/33**（其中失败项 5 个：N1 资源来源、N4 重复 transition、N5 SM 静默忽略、N6 空流程不支持、N2 输出上限缺失——失败计入覆盖、不计入通过率）
- 部分验证不计入：2 项——AI 演练真实性（产出与回归已验证，token/版本/提示原文缺失）；自动发现 probe 的冻结预算口径（无 cap 6/6 已验证、100-cap 下 4/6 已测但两种口径需并列披露）
- **证据覆盖率 = 31/33 = 94%**；通过率 = 26/31 ≈ 84%

---

## 附表一：七项软件分

| # | 维度 | 分数 |
|---|---|---:|
| 1 | 模型表达和契约 | 7.5 |
| 2 | 组合生成正确性和覆盖 | 7.0 |
| 3 | 执行可靠性、隔离与安全 | 7.0 |
| 4 | 报告、诊断和复现 | 6.5 |
| 5 | 工具自身测试完备性 | 7.0 |
| 6 | 设计、文档和后续维护 | 6.5 |
| 7 | 项目计划、风险和流程治理 | 5.5 |
| | **平均（S_software）** | **6.7** |

## 附表二：AI 五项分

| # | 维度 | 分数 |
|---|---|---:|
| 1 | AI 贡献效果 | 5.0 |
| 2 | AI 可延续资料 | 7.0 |
| 3 | AI 专用接口 | 2.0 |
| 4 | AI 质量与安全 | 4.0 |
| 5 | AI 成本与持续性 | 3.0 |
| | **平均（S_AI）** | **4.2** |

S_final = 0.85×6.7 + 0.15×4.2 = **6.33**（AI 分不抵消软件问题；本轮无软件 High）

## 附表三：24-run 结果表（Q1–Q6 × clean/mutant × 2 轮，本轮实测）

| 轮 | case | clean (MUTANT=0) | mutant (MUTANT=n) | mutant 失败点（独立抽检） |
|---:---|---|---|---|---|
| 1 | Q1 | PASS (exit 0) | PASS (exit 1) | `q_pop_empty expected result<0, actual 7` |
| 2 | Q1 | PASS | PASS | 同上 |
| 1 | Q2 | PASS | PASS (exit 1) | `q_pop_two expected result==11, actual 22` |
| 2 | Q2 | PASS | PASS | 同上 |
| 1 | Q3 | PASS | PASS (exit 1) | `q_size_nonempty expected result==0, actual 1` |
| 2 | Q3 | PASS | PASS | 同上 |
| 1 | Q4 | PASS | PASS (exit 1) | peek-consumes → size 不匹配（脚本内验证） |
| 2 | Q4 | PASS | PASS | 同上 |
| 1 | Q5 | PASS | PASS (exit 1) | `q_size_empty expected result==0, actual 1` |
| 2 | Q5 | PASS | PASS | 同上 |
| 1 | Q6 | PASS | PASS (exit 1) | push after close → 成功断言不匹配（脚本内验证） |
| 2 | Q6 | PASS | PASS | 同上 |
| 1–2 | native-q2（追加 6 run） | 2/2 PASS | 4/4 PASS | `expected result==front(items), actual 22` |

**汇总：clean 12/12、mutant 12/12、发现率 6/6=100%、误报率 0/12、单 run ≈90ms（预算 10s）、无残留进程。**

---

### 结论摘要

Manual 方案在 review-3 后 77 分钟内完成全部 7 项遗留问题的实质修复（非文档性掩盖）：guard 语义三处统一、harness 子进程/进程组/超时隔离、CLI 契约与重复声明校验、`--sequence` 供应触发入口、原生 var/list 状态变量、内嵌 24-run 统一 oracle 与生成探测、无吞错的 CI 门。本轮全部实测复现：24-run oracle 6/6 发现、0 误报、生成语义 probe 全通过、`make ci` 全绿。综合分从 4.4 升至 **6.7**（S_final 6.33），阶段门从"发布不通过"升为"发布有条件通过"。剩余工作为 3 个 Medium（`--sequence` 资源来源校验、harness 输出上限、design/ADR 同步）与治理数据回填（owner/deadline、AI token/成本），均不构成 High 阻断。
