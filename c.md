# OMX 团队交付流程概念与命令

`team-plan`、`team-prd`、`team-exec`、`team-verify`、`team-fix` 不是当前 OMX 的五个正式命令，而是对团队交付生命周期的概念性称呼。

当前 OMX 的正式主链是：

```text
$deep-interview -> $ralplan -> $ultragoal
```

需要并行时，再在执行阶段加入 `$team`。

## 概念与实际入口

| 概念 | 含义 | 交互式入口 | 终端命令 |
|---|---|---|---|
| `team-plan` | 团队任务拆分、依赖、验收标准、责任分工 | `$plan` 或 `$ralplan` | `omx ralplan run --task "..."` |
| `team-prd` | 团队共享的需求规格、PRD、测试规格 | `$deep-interview`，随后 `$ralplan` | 没有 `omx team-prd`；产物通常是 `.omx/specs/deep-interview-*.md`、`.omx/plans/prd-*.md`、`.omx/plans/test-spec-*.md` |
| `team-exec` | 多个 Agent 并行实现计划 | `$team`；持久目标场景使用 `$ultragoal` + `$team` | `omx team 3:executor "..."`；持久计划先用 `omx ultragoal create-goals ...` |
| `team-verify` | 对实现做测试、动态验证、代码审查并收集证据 | `$ultraqa`、`$code-review` | `/ultraqa --tests`、`/ultraqa --build`、`/ultraqa --custom "..."`；Team 状态用 `omx team status <name> --json` |
| `team-fix` | 根据验证或审查发现修复问题，再重新验证 | 没有独立的 `$team-fix`；继续用 `$team` 或 `$ultragoal` | 通过 Team 任务/API 下发修复任务，或用 `omx ultragoal steer ...` 调整目标，然后重新运行验证 |

## 1. `team-plan`

这是“先把团队要做什么、怎么拆、谁负责什么说清楚”。

轻量规划：

```text
$plan "实现组合测试框架"
```

需要 Planner、Architect、Critic 共识审查时：

```text
$ralplan "实现组合测试框架"
```

终端等价入口：

```bash
omx ralplan run --task "实现组合测试框架"
```

Ralplan 是 `$deep-interview` 和 `$ultragoal` 之间的共识规划阶段。

## 2. `team-prd`

这是“团队共同遵守的需求和验收文档”，不是执行命令。

需求不清楚时：

```text
$deep-interview "实现组合测试框架"
```

它会生成类似：

```text
.omx/specs/deep-interview-<slug>.md
```

再经过 Ralplan 后，通常会形成：

```text
.omx/plans/prd-*.md
.omx/plans/test-spec-*.md
```

这些文件只是规划产物，不能单独视为共识完成或执行授权。

## 3. `team-exec`

这是实际启动多个 Agent 并行工作的阶段：

```bash
omx team 3:executor "按照已批准计划实现组合测试框架"
```

Team 支持状态查询、等待和关闭：

```bash
omx team status <team-name> --json
omx team await <team-name> --timeout-ms 30000 --json
omx team shutdown <team-name>
```

如果还需要持久化目标、故事和检查点，则先创建 Ultragoal：

```bash
omx ultragoal create-goals --brief-file <brief-file>
omx ultragoal complete-goals
```

然后在合适的故事中使用 Team 并行执行。

## 4. `team-verify`

这是“实现完后证明它符合 PRD 和验收标准”。

动态验证推荐：

```text
$ultraqa --tests
$ultraqa --build
$ultraqa --custom "运行组合测试并检查内存错误"
```

代码审查推荐：

```text
$code-review
```

`$code-review` 主要做静态质量、安全、性能和架构审查；`$ultraqa` 负责可运行的测试、对抗性场景和端到端验证。

## 5. `team-fix`

当前没有独立的 `team-fix` 命令。它表示：

```text
发现问题
 -> 把修复任务交给 Team 或当前执行 Agent
 -> 修复
 -> 重新运行 team-verify
```

例如重新启动一个明确的修复任务：

```bash
omx team 2:executor "修复 UltraQA 发现的内存越界问题，并补充回归验证"
```

如果是持久目标计划中的范围、顺序或目标变化，则使用：

```bash
omx ultragoal steer \
  --kind revise_pending_wording \
  --evidence "验证发现原验收条件遗漏异常输入" \
  --rationale "修正目标后重新执行验证" \
  --json
```

## 推荐流程

```text
team-plan  -> $plan / $ralplan
team-prd   -> $deep-interview + $ralplan 生成 spec/PRD/test-spec
team-exec  -> $ultragoal（可选）+ $team
team-verify-> $code-review + $ultraqa
team-fix   -> $team 或 $ultragoal steer，然后重新 $ultraqa
```

不要直接输入：

```bash
omx team-plan
omx team-prd
omx team-exec
omx team-verify
omx team-fix
```

这些名称在当前 OMX `v0.21.3`、catalog `2026.02.28.1` 中都不是正式顶层命令。

## 本地依据

- 项目流程选择：[omx.md](omx.md:60)
- Team 启动和管理：[team/SKILL.md](/home/hpvr/.codex/skills/team/SKILL.md:10)
- Ralplan 共识规划：[ralplan/SKILL.md](/home/hpvr/.codex/skills/ralplan/SKILL.md:6)
- Deep Interview 需求规格产物：[deep-interview/SKILL.md](/home/hpvr/.codex/skills/deep-interview/SKILL.md:337)
- Ultragoal 持久目标与 Team 桥接：[ultragoal/SKILL.md](/home/hpvr/.codex/skills/ultragoal/SKILL.md:18)
- UltraQA 验证、诊断和修复循环：[ultraqa/SKILL.md](/home/hpvr/.codex/skills/ultraqa/SKILL.md:35)

