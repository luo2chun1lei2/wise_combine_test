# OMX Skills

来源：`omx list --json`  
Catalog：`2026.02.28.1`  
说明：已过滤下线项；保留 `active`、`alias`、`merged` 和 `internal` 项。

## Execution

| Skill | 状态 | 简要功能 |
|---|---|---|
| `autopilot` | active | 自动编排 `deep-interview -> ralplan -> ultragoal` |
| `team` | active | 创建和协调多个并行 Agent |
| `ultraqa` | active | 执行对抗式动态端到端 QA |
| `autoresearch` | active | 执行持久化、验证门控的研究循环 |
| `performance-goal` | active | 执行评估器门控的性能优化目标 |
| `ultragoal` | active | 创建和执行持久化多目标计划 |

## Planning

| Skill | 状态 | 简要功能 |
|---|---|---|
| `plan` | active | 执行轻量级任务规划 |
| `ralplan` | active | 执行 Planner、Architect、Critic 共识规划 |
| `deep-interview` | active | 通过苏格拉底式访谈澄清需求歧义 |
| `best-practice-research` | active | 基于官方或上游资料开展定向调研 |

## Shortcut

| Skill | 状态 | 简要功能 |
|---|---|---|
| `analyze` | active | 只读深度分析代码仓库 |
| `ai-slop-cleaner` | active | 清理和重构 AI 生成的低质量代码 |
| `code-review` | active | 执行综合代码审查，路由到 `code-reviewer` |
| `visual-ralph` | active | 通过视觉 QA 迭代前端实现，路由到 `designer` |
| `design` | active | 管理仓库内的 `DESIGN.md` 设计流程，路由到 `designer` |
| `git-master` | alias | Git 提交、历史、rebase、bisect 等工作流 |
| `ask` | active | 调用本地外部顾问并保存结果工件 |

## Utility

| Skill | 状态 | 简要功能 |
|---|---|---|
| `cancel` | active | 取消正在运行的 OMX 模式 |
| `doctor` | active | 诊断和修复 OMX 安装问题 |
| `wiki` | active | 管理持久化 Markdown 项目 Wiki |
| `skill` | active | 列出、添加、删除、搜索和编辑本地 skill |
| `hud` | active | 显示或配置 OMX HUD |
| `omx-setup` | active | 安装和配置 OMX |
| `configure-notifications` | active | 配置 OMX 通知 |
| `configure-discord` | merged | 已合并到 `configure-notifications` |
| `configure-telegram` | merged | 已合并到 `configure-notifications` |
| `configure-slack` | merged | 已合并到 `configure-notifications` |
| `configure-openclaw` | merged | 已合并到 `configure-notifications` |
| `worker` | internal | OMX Team 内部 Worker 协议 |
