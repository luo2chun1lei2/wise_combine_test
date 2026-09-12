# ADR 0003：报告 v2 与重放格式

状态：v2 envelope 基础已实现；完整 payload schema 与 replay 尚未实现。本文不改变当前 v1 命令行为。

## 数据契约

v2 使用外层对象 `schema_version: 2`、`payload`、`integrity`，拒绝额外字段。
`payload` 是保存完整执行输入与结果的 JSON **字符串**，不是外层嵌套对象。
`integrity` 只允许 `algorithm: "sha256"` 和 `digest`（64 位小写十六进制）。
摘要输入为 JSON 解码后 payload 字符串的原始 UTF-8 字节，不包括 BOM 或额外换行。
外层缩进、字段排序、等价 JSON 字符串转义不影响摘要；payload 内部任意字节变化均改变摘要。
因此不依赖跨实现浮点数规范化，也不使用 `std::hash` 或非密码学指纹。

payload 解码后包含以下字段，缺失、类型错误及额外字段均拒绝：

| 字段 | 内容 |
| --- | --- |
| model | 完整版本 1 规范对象，包含 seed、limits；按现有规范校验 |
| generator | 固定策略版本 `seeded-dfs-v1` 和原始终止状态 |
| flow | flow_id 与完整 transition_ids，包括首次失败后的计划步骤 |
| adapter | 原始程序绝对路径、程序文件 SHA-256、arguments 字符串数组、绝对工作目录 |
| runtime | step_timeout_ms、total_timeout_ms、output_limit；固定环境 PATH=/usr/bin:/bin、LC_ALL=C |
| result | 原始流程状态与已执行 steps，包含实际 args、expected/observed、returns、stderr 和退出/信号信息 |

不得收集整个宿主环境或凭证。参数和模型可能包含敏感数据，文档必须提醒用户审查报告后再共享。

## 命令契约

`verify-report-v2 REPORT` 验证 v2 envelope；v1 只验证结构，v2 在解析 payload
前验证 SHA-256，再验证模型、流程及结果的相互对应关系（后续 payload schema 阶段）。成功输出必须明确区分
`integrity_verified`，不得将 v1 返回结果描述为哈希校验通过。

`replay REPORT --adapter EXEC --reports NEW_DIR --run-id ID` 只接受完整 v2。
调用方必须显式给出 EXEC；报告中的程序路径不能成为自动执行入口。
EXEC 必须通过现有 allowlist，且文件 SHA-256 与报告一致。继承记录的参数、工作目录、
固定环境和运行限制；重放前检查工作目录存在。拒绝缺失字段、未知版本、模型/流程矛盾、
不匹配程序摘要，且不得在验证失败时启动 adapter。

重放只执行保存的这一条完整流程，不重新运行生成器搜索；仍按原模型进行参数绑定和
状态判定，不能用报告里的 observed 值伪造响应。新报告写入新目录，禁止覆盖输入报告。
保持现有失败退出码；0 表示本次执行成功，不表示与历史所有非确定性数据完全相同。

adapter 外部文件/服务状态不包含在报告中。调用方须恢复与原始执行一致的前置状态；
无法恢复时不能保证复现。不得默认删除或覆盖 adapter 状态文件。

## 安全与验收

SHA-256 检测未同步修改摘要的内容变更，不证明来源可信：能同时修改 payload 与 digest
的人仍能生成一致报告。来源认证需要另行设计签名或可信外部摘要，本轮不冒称具备该能力。

实现顺序与可观测验收：

1. SHA-256：空串、abc、跨块及二进制已知向量测试，与系统 sha256sum 交叉核对。
2. v2 读写：正常 round-trip；payload/摘要修改、重复字段、非法 UTF-8、未知算法/版本拒绝。
3. 模型/流程：拒绝未知 transition、断开的状态路径、违反 before/argument、超过限制的流程；
   支持空流程、循环、失败前缀和完整计划序列的区分。
4. CLI 重放：原规范删除后仍可重放；clean 通过、真实 mutant 重现、adapter 摘要不匹配拒绝。
5. 副作用：拒绝时 adapter 标记文件不存在；输入报告保持字节不变；错误工作目录与输出路径
   给出非零退出码；外部状态恢复由用户显式控制。
6. 双语 README、Debug/sanitizer、实际 CLI 场景和覆盖率门禁同时更新并留存证据。
