# LazyCodex 整改与增强记录

本文件记录 `wise_combine_test.evaluate` 对 LazyCodex 的独立复评意见，作为后续开发的工作依据。本文只记录计划，不代表其中的增强已经实现。

评审基线：分支 `layzcodex`，提交 `52a7fa5`。评审确认当前实现的基础状态图、typed function、argument/before 关系、外部 adapter 隔离、超时/崩溃/输出上限处理和 19/19 Debug、19/19 ASan/UBSan/LSan 测试均可用。

## 当前边界

| 能力 | 状态 | 说明 |
| --- | --- | --- |
| 状态图、状态迁移、函数签名 | 支持 | 已有 schema、校验和生成测试 |
| producer-consumer 参数绑定 | 支持 | 返回值会注入消费者参数，缺少来源的必需参数会被拒绝 |
| `before` 顺序关系 | 支持 | 校验环和非法引用 |
| 有界确定性 DFS | 支持 | 受 `max_cases`、`max_steps` 限制，允许有界非自环重走 |
| seed 驱动的随机策略 | 基础支持 | 多候选流程按 seed 稳定打乱；单候选流程结果不变 |
| 完整 adapter JSON 解析 | 待增强 | 当前实现仍有字符串扫描路径 |
| 报告 replay、篡改校验 | 待增强 | 当前报告不足以独立重现复杂失败 |
| guard、mutex、parallel、count/value | 未支持 | 是否纳入产品需先冻结需求 |
| 多对象交互、负向流程 | 未支持 | 不应在文档中暗示已实现 |

## 已完成增量

- P0 报告路径错误（基础完成）：`0c54f45`、`6107d79`、`aa290a3`。目录创建失败、流程报告打开/写入失败、summary 打开/刷新失败均返回退出码 `5`；权限和磁盘故障注入仍是环境相关的补充场景。
- 参数来源校验：`3627992`。缺少常量和 producer 绑定的必需 transition 参数会在模型校验阶段拒绝，并有模型回归测试。
- 项目知识库同步：`f2b7ff1`。`AGENTS.md` 已更新为当前实现目录、命令和维护边界。
- Adapter 状态分类：`8e79eb5`。合法 `error`、`mismatch` 和未知状态已有独立回归；完整 JSON 语法解析仍待增强。
- 非自环重复：`acc1c4d`。合法状态循环可在 `max_steps` 内重走，并有 `A→B→A` 回归测试。

## P0：发布阻断

### 报告目录和报告写入失败

状态：基础完成。目录创建失败导致异常终止已修复，报告文件和 summary 的打开、写入、刷新失败均有显式检查。原问题是 `--reports` 指向普通文件等不可创建路径时，进程会以 134 终止。

整改：CLI 和 report 层均检查写入状态并返回退出码 `5`，输出路径和原因，避免写入失败仍输出成功 summary。

验收：普通文件路径场景已加入 CTest，Debug 与 ASan/UBSan/LSan 均通过 20/20；权限不足和不可覆盖文件仍列为后续补充场景。

## P1：契约和可复现性

### Adapter 响应使用完整 JSON 解析

复用或抽取严格 JSON parser，校验 protocol、status、observed_state、returns、stderr 的类型和允许字段；拒绝未知字段、重复字段、额外 JSON、尾随非空白、错误类型和超过输出上限的响应。补充字段重排、空白、转义、Unicode、嵌套 returns 和 malformed 测试。

### Transition 参数来源严格校验

状态：基础缺失来源校验已完成；稳定诊断码、JSON Pointer 和更丰富的类型边界仍待增强。

在模型阶段拒绝缺少必需参数、未知参数名、常量类型不匹配、缺少 producer/外部输入来源和多个 producer 绑定同一参数。诊断包含稳定错误码和 JSON Pointer。

### 明确或实现 seed 语义

状态：基础策略已完成（当前提交）；复杂覆盖策略和未覆盖原因仍待增强。

推荐实现可复现伪随机候选排序：相同规范、seed 和限制必须字节一致；不同 seed 在存在多个合法候选时产生可解释差异。若暂不实现，则从文档中删除“seed 影响生成”的暗示并明确其当前无效。

### 报告元数据与 replay

报告应包含 schema/model 版本、规范哈希、seed、limits、生成策略、flow 序列、实际 args、expected/actual、adapter 信息、环境摘要、稳定 report ID、截断标记和退出信号。增加 `replay` 与报告校验入口，覆盖正常重放、输入篡改、版本不匹配和哈希不一致。

### 同步项目知识库

状态：已完成基础同步；后续新增模块仍需按提交同步维护。

更新 `AGENTS.md` 中过期的“requirements-only/no implementation”描述，补充当前目录、构建命令、测试命令、证据路径和明确不支持的语义。同步更新 `README.md` 与 `README.zh.md`。

## P2：生成器和长期回归

### 非自环重复、空流程和子集

状态：非自环有界重走已完成；重复次数的独立配置、空流程和前缀流程仍待增强。

先冻结 transition 是否可重复的产品语义，再增加访问次数或 transition repeat 限制。测试非自环循环、重复上限、去重、`max_steps` 共同限制、空流程和前缀流程。

### 接入 Q1–Q6 回归资产

将评审目录中的模型、adapter、fixture 和 oracle 转为项目内的 `tests/evaluation/` 或等价入口。长期回归必须区分模型表达、生成覆盖、执行判错和报告复现，并记录发现率、误报率、无法表达数和未执行数。

### 需求、设计和测试矩阵

建议新增：

- `docs/architecture.md`
- `docs/requirements-matrix.md`
- `docs/test-matrix.md`
- `docs/adr/0001-json-schema-and-adapter.md`
- `docs/adr/0002-generation-seed-policy.md`
- `docs/adr/0003-report-and-replay-format.md`

每条需求应有唯一编号、实现位置、测试、证据、状态，并区分核心契约、可选扩展、明确非目标和未验证项。

## P3：可选表达能力扩展

只有在需求冻结后实施，建议顺序为 guard/count/value、mutex、资源生命周期、parallel、多对象交互和负向流程。每项扩展必须同时更新 schema、model、generator、runtime、report、CLI、测试矩阵以及中英文 README，并提供正常、边界、非法和组合场景。

## 建议开发批次

1. 报告路径错误处理和稳定退出码。
2. 完整协议解析与 transition 参数来源校验。
3. seed、非自环重复、空流程和子集语义。
4. 报告元数据、replay 和篡改校验。
5. Q1–Q6 项目内回归入口。
6. AGENTS、需求矩阵、设计 ADR、测试矩阵和发布/回滚记录。
7. 冻结需求后再实现高级约束和多对象能力。

每个批次都应遵守：先记录失败场景，再做最小修改；补充自动测试和真实 CLI 验证；运行 sanitizer；更新英文和中文 README；以独立提交交付。

## 暂不纳入本轮实现的事项

- GUI、网络服务、分布式执行和插件市场；
- 未经需求确认的 guard/mutex/parallel/count/value 语义；
- 把评审 adapter 的外部断言当作产品原生能力；
- 把给定流程的执行发现率当作生成器自动覆盖率；
- 声称在有界生成之外实现穷举覆盖。
