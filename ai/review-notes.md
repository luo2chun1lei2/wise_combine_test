# Manual 方案复评说明

评审对象：`wise_combine_test.manual`。

## 版本与快照

- 建议复评 tag：`v0.3-review`
- 提交历史：见 `git log --oneline`
- 台账：
  - AI 使用记录：`ai/AI-USAGE.md`
  - AI 成本基线：`ai/AI-COST.md`
  - 任务台账：`ai/task.md`
  - 能力边界：`doc/capabilities.md`
  - 治理/阶段门：`doc/governance.md`
  - 资料索引：`ai/index.md`

## 复现命令

```sh
make ci
make oracle
```

`make ci` 包含干净构建、`make test` 和 `make oracle`。`make oracle` 包含 24-run 统一 oracle 和 generation probe。

## 与 review-3 的对照结论

| 类别 | 改善结果 |
|---|---|
| guard 生成/执行/harness | 统一求值，未绑定变量跳过或报错 |
| 执行隔离 | harness 使用子进程/进程组和 `--timeout` |
| CLI 契约 | dfs/max-cases/max-length/严格数字解析均修复 |
| 重复声明 | type/value/resource/class/setup/state/action 拒绝重复 |
| 失败传播 | 测试脚本不再 `|| true` 吞错 |
| 统一 oracle | 执行 24/24 PASS，native-q2 6/6 PASS |
| 生成自动发现 | generation probe 6/6 发现 |
| 报告 | model_hash/seed/version/case_ids/actual 已补齐 |
| 函数 count/value | 新增 `var`/`list`/`update` 和 `len`/`front` |
| 文档/治理 | capabilities/governance/AI 成本/安全清单已建立 |

## 仍需外部回填项

- `ai/AI-COST.md` 的 token、费用、人工审查和返工时间需要从真实供应商账单/记录回填。
- `doc/governance.md` 的 owner/deadline 需要后续逐任务实填。
- 复评方需按相同标准重新执行 `make ci` 和 `make oracle`，以更新阶段门结论。

