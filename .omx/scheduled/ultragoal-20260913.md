# Scheduled Ultragoal Run — 2026-09-13 10:00 Asia/Shanghai

继续执行本仓库 `.omx/ultragoal` 中的计划任务，直到所有可执行目标完成。遵循
`$ultragoal` skill 和仓库 `AGENTS.md`：逐故事验证、写入 durable checkpoint、运行
必要测试，并提交和推送每一步的变更。

如果同一个问题（相同根因/阻断条件）连续出现三次，立即停止继续尝试；将该问题标记
为 blocked，并在最终消息中分析复现证据、最可能根因、已尝试的恢复路径和需要的外部
决策。不要把不同问题合并计数，也不要在第三次之后继续修改。

结束条件：所有 durable goals 完成且最终验证通过，或触发上述三次相同问题退出规则。
