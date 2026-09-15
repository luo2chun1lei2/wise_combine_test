# 第四轮评审交叉复核记录（汇总者）

复核人：本轮汇总者（非四个项目级 subagent）。目的：按《评价标准.md》v2.2 §5.2，汇总者核对关键承重结论后再统一严重程度与评分。以下每项均为独立于 subagent 的命令级复测。

## C1. Plan/Goal 冻结口径 clean 误报（N1，影响排名的关键结论）— 复现成功

用 review-3 冻结资产（Q2.ct + adapter，MUTANT=0）按冻结协议原样运行当前版本 `src/out/wise_combine_test`（HEAD eb219c6）：

- 命令要点：`wise_combine_test Q2.ct --adapter adapter-0 --adapter-arg q2.state --max-depth 8 --max-flows 100 --report json`
- 结果：**exit=4**，passed=45，failed=55（clean SUT 被判失败，误报复现）。
- 成分分析：55 条失败流程**全部**含 `q_pop_first`；示例 flow `q_open,q_open,q_push1,q_push2,q_push2,q_pop_first,q_pop_first`，detail `return mismatch for q_pop_first: expected 11, got 22`——重复调用第二次返回值与按函数名单值 expect 冲突，证实 N1 机制（生成器新增的重复/子集能力与单值 expect 判定语义不兼容），非 SUT 缺陷。
- 同时证实 N3：输出文件尾部追加 `# warning: generation was truncated by --max-flows`，导致 `--report json` 输出为非法 JSON（json.load 报 Extra data）。

## C2. LazyCodex 模型阶段拒绝缺参来源（review-3 BLOCK 项修复声明）— 复现成功

从 `tests/fixtures/relation_valid.json` 删除全部 `argument` 关系后 `build-coverage/wise-combine validate`：

- 输出：`spec error: semantic validation failed [invalid_argument]: transition is missing a value for parameter: consume.input`
- **exit=2**（稳定错误码 + 稳定 code name），validate 与 run 均在模型阶段拒绝，不启动子进程。符合专项标准 §2.1“来源缺失必须在模型阶段拒绝”。

## C3. OMX 严格数值解析与重复声明拒绝（源码级）— 确认

- `src/wct.c:197` `parse_unsigned_strict()`（strtoull + end 完整性）存在，且 `:326` schema 指令、`:332` contract argc 均改走该函数；`:316` 起有 `schema_seen/state_graph_seen` 重复声明计数（`duplicate schema; first declared at line N`）。与 subagent 命令级实测结论一致。

## C4. Manual 自身测试与内嵌 oracle — 复现成功

- `make test`：exit 0，输出 PASS。
- `make oracle`：exit 0，日志含 30 个 PASS（24-run 统一 oracle + native-q2 6 run）。review-3 的“统一 oracle 未在预算内完成”已解除。

## C5. 版本与工作区完整性

四个项目评审后 `git status --short` 均为空，HEAD 与 subagent 报告一致（omx 1faa41e / lazycodex b67e24f / manual 7eb9ce1 / plan_goal eb219c6）。

## 严重程度统一裁定

- Plan/Goal N1 维持 **Medium（偏高）**：按通用标准 §7，High 定义为“错误结果被判为成功”，N1 是正确结果被判失败（误报），不满足 High 字面定义；但其发生在默认模式且 README 未警告，列为 Plan/Goal 发布阶段门的第一条件项。若后续将默认函数流程模式冻结为核心契约，应升级为 High。
- OMX M1（超时不清理进程组/后代）维持 **Medium**：违反专项标准 §2.3 明确要求。
- Manual N1（`--sequence` 不校验资源来源，响亮编译错误而非误判通过）、N2（harness 无输出上限）、N3（设计/ADR 未同步）维持 **Medium**。
- 其余 Low 项维持各 subagent 判定。

## 评分一致性核对

四个 subagent 的锚点使用一致（9-10/7-8/4-6/0-3），相对严重度与证据量对应关系合理；未发现同类问题在不同项目被判不同严重度的情况。汇总者接受四份项目级评分，不做调整。
