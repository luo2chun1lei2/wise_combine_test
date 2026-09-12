# JSON 报告结构说明

本文件描述 `--json` 和 `--harness-json` 输出的稳定字段。字段版本通过顶层 `version` 标识，当前为 `0.2`。

## 函数模型 `--json`

顶层字段：

- `kind`：`"function"`。
- `version`：工具报告版本。
- `model_hash`：输入模型文本的 FNV-1a 64 位哈希，十六进制。
- `seed`：本次生成使用的随机种子。
- `types`、`values`、`resources`、`functions`：模型实体数量。
- `sequences`：函数调用序列文本数组。
- `case_ids`：与 `sequences` 等长的稳定用例 ID 数组，形如 `fn-<hash8>`。
- `negative_sequences`：负向序列文本数组。
- `coverage`：可选，`--coverage` 时输出函数/函数对/t-way 覆盖。
- `errors`：模型语义错误数组。

## 状态机模型 `--json`

顶层字段：

- `kind`：`"state_machine"`。
- `version`：工具报告版本。
- `model_hash`：输入模型文本的 FNV-1a 64 位哈希。
- `seed`：随机算法使用的种子。
- `machine`、`states`、`events`、`transitions`：模型摘要。
- `paths`：状态机路径文本数组。
- `case_ids`：与 `paths` 等长的稳定用例 ID 数组，形如 `sm-<hash8>`。
- `skipped_guards`：因 guard 未绑定被跳过的转换说明。
- `truncated`：可选，`tour` 因 `--max-length` 未覆盖全部转换时为 `true`。
- `uncovered_transitions`：可选，`truncated` 时未覆盖转换数组。
- `coverage`：可选，覆盖统计。
- `negative`：可选，负向场景数组。
- `errors`：模型语义错误数组。

## 状态机 `--events --json`

顶层字段：

- `kind`：`"state_machine_execution"`。
- `machine`：状态机名称。
- `trace`：已执行转换文本数组。
- `actions`：动作轨迹数组。
- `final`：最终活动叶子状态。
- `failed`：是否失败。
- `failure`：失败原因；guard 变量未绑定时包含变量名。

## harness JSON 失败记录

`--harness-json` 生成的 harness 在失败时输出单行 JSON：

- `kind`：`"failure"`。
- `seq`：序列编号。
- `step`：失败函数名。
- `expected`：期望表达式或状态。
- `actual`：实际返回值或状态观察值。

失败、崩溃或超时会导致 harness 非零退出；超时额外输出 `TIMEOUT <seq>`。
