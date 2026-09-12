# OMX 评审改进方案

> 来源：`wise_combine_test.evaluate‌/wise_combine_test-review-1.md`、
> `wise_combine_test-review-2.md`、`wise_combine_test-review-3.md` 及其 OMX 复评附件。
>
> 本文记录方案，不代表其中的代码修复已经完成。当前版本仍按评审结论维持“发布暂缓”。

## 1. 当前结论与优先级

OMX 的软件复评平均分为 **8.4/10**，需求与设计通过，编码和测试有条件通过，
发布暂缓。主要问题按优先级如下：

| 优先级 | 问题 | 影响 | 发布要求 |
| --- | --- | --- | --- |
| P0 | 数字 token 接受尾随字符 | 削弱 DSL、trace 和 replay 的完整性 | 发布前必须修复 |
| P1 | 重复 `state_graph`/`relation_graph` 静默覆盖 | 模型可能被悄悄改变 | 发布前必须修复 |
| P1 | release checklist 与 I15 `PASS` 状态冲突 | 发布结论不可审计 | 发布前必须统一 |
| P2 | 覆盖率低且 fork 子进程归集策略不清 | 全面性证据不足 | 建立关键分支门禁 |
| P2 | AI provenance、成本和安全记录不足 | 影响 AI 持续维护可信度 | 独立增强轨道 |

## 2. 阶段 0：冻结能力边界和发布基线

先为每项能力标记 `Required`、`Advertised`、`Optional` 或 `Unsupported`，避免文档
把未实现能力写成已交付能力。

当前应明确支持：

- 状态图组合测试；
- relation DAG、确定性/seed 调度；
- 前序调用结果作为参数；
- contract、expect、snapshot/restore；
- 进程隔离、超时、trace/replay。

当前应明确不承诺：

- 自动搜索任意参数空间；
- 推断用户未声明的关系；
- 通用上下文对象（除非以后提升为必需能力）；
- relation 隔离场景自动提交任意 callback 副作用。

建议维护以下单页索引：

- `.omx/current-status.md`：只描述当前版本状态；
- `.omx/capability-matrix.md`：能力分类和证据链接；
- `.omx/release-blockers.md`：阻断项、负责人、期限和关闭证据。

## 3. 阶段 1：严格数值解析（P0）

### 3.1 范围

- `src/wct.c:242,248`；
- `tools/wct_cli.c:214-238`。

### 3.2 方案

统一增加严格解析函数，例如 `parse_uint_strict`、`parse_size_strict` 和
`parse_hex_u64_strict`，使用 `strtoul`/`strtoull`，并检查：

1. token 非空；
2. `end` 指针确实到达 token 末尾；
3. 溢出和目标类型范围；
4. 不接受非法符号、非法前缀或尾随字符；
5. 十进制与十六进制字段使用明确的解析规则。

### 3.3 必测回归

以下输入必须拒绝：`schema 1x`、`contract call 0x`、`seed 123x`、`seed 0x`、
`steps 1x`、非法 digest、超出 `UINT_MAX`/`SIZE_MAX` 的值。合法边界值必须继续接受。
被篡改数字字段的 trace 必须导致 replay 失败。

## 4. 阶段 2：拒绝重复顶层声明（P1）

parser 增加 `state_graph_seen`、`relation_graph_seen` 及首次声明行号。第二次声明
立即失败，并报告当前行号和首次声明行号，例如：

```text
line 8: duplicate state_graph; first declared at line 2
```

同时固定重复 state/call/transition ID、重复 relation edge、重复 schema 以及缺失
模式声明的契约，并在 API、DSL 和 README 中说明。

## 5. 阶段 3：关键分支测试矩阵（P1/P2）

不以机械达到 80% 覆盖率为第一目标；先覆盖发布关键分支。

### Parser

空文件、缺失/错误 schema、超长行、未知 directive、重复声明、重复 ID、空字段、
null 输出指针和 allocation failure。

### State runner

线性与分支路径、不可达 edge、cycle limit、expectation mismatch、callback 失败、
失败后的状态回滚、snapshot/restore 失败、timeout、crash、子进程非零退出和成功提交。

### Relation runner

DAG 与环检测、确定性 tie-break、多 flow、flow limit、`uncovered`、producer 参数
绑定、arity/type mismatch、result contract mismatch、空/子集/重复调用。

### Trace/replay

合法 trace、缺失/重复字段、字段顺序变化、非法数字、model/IR/metadata/step digest
不匹配、tampered trace、trace 不可写和 replay 临时文件失败。

### 覆盖率归集

明确 fork 子进程是否产生独立 `.gcda`、是否统一归集，以及报告是否包含 CLI、库和
子进程路径。README 必须区分当前实测值、关键分支覆盖率、可选 80% 目标和子进程
归集范围。

## 6. 阶段 4：统一发布状态和证据索引（P1）

`docs/release-readiness.md` 中的待补项目与 `evidence/iter-15/summary.json` 的
`status: PASS` 必须拆分为：

1. **当前发布状态**：当前是否 BLOCKED；
2. **历史迭代状态**：I0–I15 原始 evidence，只作审计记录；
3. **自动生成结论**：根据最新 commit、必需 artifact、边界测试、sanitizer、
   Valgrind、coverage、replay 和 manifest hash 重新判定。

不要改写历史 I15 证据来掩盖新发现。修复后应生成新的迭代目录，包含 summary、
manifest、边界日志、sanitizer/Valgrind/coverage、Q1–Q6、generation probes 和
tampered replay 结果。

## 7. 阶段 5：组合语义回归（P2）

将评审资产中的 Q1–Q6 控制组、mutant 组、cycle/subset/repeat probes、参数绑定、
failed-transition atomicity、branch replay 和 trace replay 正式纳入回归。

报告必须区分：

- generator coverage：生成器实际覆盖的流程；
- executor fault detection：执行器发现已知 mutant 的能力；
- oracle coverage：统一有状态 oracle 覆盖；
- replay determinism：重复运行和回放一致性。

mutant 全部被发现只能证明执行器的判错能力，不能单独证明生成器枚举完整。

## 8. AI/OMX 编排增强（独立轨道）

AI 改进不应掩盖当前 P0/P1 软件问题。若继续宣称 AI 可持续维护能力，应记录：

- 模型供应商、名称、版本；
- prompt 版本；
- 输入资料路径及 hash；
- 输出 artifact；
- 人工采纳、修改、拒绝和返工次数；
- 运行时间、token/成本估算；
- 使用的工具和权限范围。

建议目录：

```text
.omx/ai/
  runs.jsonl
  prompts/
  sources/
  decisions/
  cost-report.tsv
```

还应增加 prompt injection、凭证泄漏、越权文件访问、未授权 `.omx` 修改、模型升级
离线重放和第三方许可证审查。需求、设计、ADR、代码、测试、evidence 和发布状态
应由单一索引关联。

## 9. 推荐执行顺序与发布门

1. 严格数字 token 解析；
2. 拒绝重复顶层声明；
3. 补 parser/replay 边界回归；
4. 补错误路径和 fork 覆盖归集；
5. 重新运行 sanitizer、Valgrind、coverage、Q1–Q6、generation probes 和
   tampered replay；
6. 重新生成 manifest 与当前 release summary；
7. 建立 current-status 索引并消除历史状态冲突；
8. 最后补 AI provenance、成本、安全和升级重验。

在第 1–4 项完成前，发布状态保持：

```text
Release: BLOCKED
```

完成修复、边界回归、证据重生成并通过独立复评后，才可考虑：

```text
Release: CONDITIONALLY PASS
```

## 10. 每阶段交付纪律

每个阶段独立提交并推送，提交信息建议使用：

```text
fix: enforce strict numeric token parsing
fix: reject duplicate top-level declarations
test: add parser and replay boundary matrix
test: document child-process coverage aggregation
docs: reconcile release status and historical evidence
chore: add AI provenance and cost ledger
```

每次提交前运行针对性测试和 `make test`；适用时运行 sanitizer、Valgrind、coverage、
测量和 replay，并把源代码、文档、`.omx`、`.agents` 等非构建产物一并纳入提交。
