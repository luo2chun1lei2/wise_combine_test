# OMX Agents

来源：`omx list --json`  
Catalog：`2026.02.28.1`  
说明：已过滤下线 agent；保留 `active`、`merged` 和 `internal` 项。

## Build

| Agent | 状态 | 简要功能 |
|---|---|---|
| `explore` | active | 快速检索代码仓库并定位相关文件和符号 |
| `analyst` | active | 澄清需求、验收标准和约束 |
| `planner` | active | 设计任务分解、执行顺序和风险控制 |
| `architect` | active | 负责系统设计、边界和长期架构权衡 |
| `debugger` | active | 进行根因分析、故障隔离和回归诊断 |
| `executor` | active | 执行代码实现、重构和功能开发 |
| `team-executor` | internal | 执行受监督的团队协作任务 |
| `verifier` | active | 收集完成证据并验证测试充分性 |

## Review

| Agent | 状态 | 简要功能 |
|---|---|---|
| `style-reviewer` | merged | 已合并到 `code-reviewer`，负责代码风格审查 |
| `quality-reviewer` | merged | 已合并到 `code-reviewer`，负责代码质量审查 |
| `api-reviewer` | merged | 已合并到 `code-reviewer`，负责 API 契约审查 |
| `performance-reviewer` | merged | 已合并到 `code-reviewer`，负责性能审查 |
| `code-reviewer` | active | 执行综合代码审查并识别风险 |

## Domain

| Agent | 状态 | 简要功能 |
|---|---|---|
| `dependency-expert` | active | 评估第三方依赖、SDK 和版本选择 |
| `test-engineer` | active | 设计测试策略、覆盖率和稳定性方案 |
| `quality-strategist` | merged | 已合并到 `verifier`，负责质量策略 |
| `designer` | active | 负责 UX/UI 架构、视觉设计和可访问性 |
| `writer` | active | 编写文档、迁移说明和用户指南 |
| `qa-tester` | merged | 已合并到 `test-engineer`，负责 QA 测试 |
| `git-master` | active | 负责 Git 提交策略、历史和分支操作 |
| `code-simplifier` | internal | 在不改变行为的前提下简化代码 |
| `researcher` | active | 调研外部文档、参考资料和开源实现 |

## Product

| Agent | 状态 | 简要功能 |
|---|---|---|
| `product-manager` | merged | 已合并到 `analyst`，负责产品需求分析 |
| `ux-researcher` | merged | 已合并到 `designer`，负责用户体验研究 |
| `information-architect` | merged | 已合并到 `designer`，负责信息架构 |
| `product-analyst` | merged | 已合并到 `analyst`，负责产品分析 |

## Coordination

| Agent | 状态 | 简要功能 |
|---|---|---|
| `critic` | active | 批判性检查计划、设计和技术方案 |
| `vision` | active | 分析图片、截图、图表和视觉证据 |
