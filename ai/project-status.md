# 项目状态与发布门禁

## 当前状态

- 分支：`plan_goal`
- 阶段门：发布前补救项基本完成，剩余项见 `defects.json`。
- 当前可重复验证命令：
  - `make check`
  - `make asan`
  - `make coverage`
  - `./ai/project-gate.sh`

## 发布门禁

提交前应至少运行：

```text
./ai/project-gate.sh
```

需要完整内存检查时运行：

```text
./ai/project-gate.sh --asan
```

## 当前明确边界

- direct / standalone 仅支持无参 C ABI。
- 强类型 adapter 标量仍为后续扩展。
- fork/exec 子进程覆盖自动归集仍为后续工程项。
- AI 模型、提示、token、费用基线未采集。
