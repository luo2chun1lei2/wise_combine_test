# 架构说明

## 数据流

```text
JSON specification
        |
        v
spec parser + normalization
        |
        v
typed model + semantic validation
        |
        v
seeded bounded generator
        |
        v
flow executor -- execve --> allowlisted adapter
        |
        v
JSON/text reports + CLI measurements
```

`src/spec` 负责 RFC 8259 JSON、版本和规范化；`src/model` 保存状态、函数、参数、返回值和关系，并执行语义校验；`src/generate` 按状态、关系、seed 和全局限制生成流程；`src/runtime` 每步启动隔离的 adapter，负责协议、超时、输出上限和 producer 返回值注入；`src/report` 生成 JSON/text 报告；`src/cli` 映射命令和退出码。

## 执行边界

- adapter 通过 `execve` 启动，不经过 shell。
- 环境固定为 `PATH=/usr/bin:/bin`、`LC_ALL=C`。
- adapter 可执行文件必须通过 allowlist。
- 每步和整个 flow 都有超时，输出有合计上限。
- 超时会清理进程组；协议错误、崩溃和 mismatch 不会被当作成功。
- 每个 transition 的必需参数必须来自常量或 argument relation。

## 当前设计决策

| 决策 | 当前做法 | 后续影响 |
| --- | --- | --- |
| 输入格式 | version 1 JSON | schema 变更需增加版本策略 |
| 生成策略 | seeded DFS、稳定候选顺序 | 复杂覆盖策略需要独立设计 |
| 执行方式 | 每步独立 adapter 进程 | 有状态 SUT 需外部状态桥或未来持久会话 |
| 关系传值 | producer returns 注入 consumer args | 需要保持类型严格匹配 |
| 失败分类 | parse/generation/mismatch/runtime/usage | 新错误必须更新 CLI 和双语 README |

## 明确非目标

当前不包含 GUI、网络服务、分布式执行、插件市场、任意动态符号调用和参数扫描式完备性分析。guard、mutex、parallel、count/value、多对象和负向流程在需求冻结前均视为未支持。
