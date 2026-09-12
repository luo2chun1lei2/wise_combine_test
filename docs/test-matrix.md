# 测试矩阵

## 当前自动化矩阵

| 层级 | 正常 | 边界/非法 | 故障注入 | 当前入口 |
| --- | --- | --- | --- | --- |
| model | 合法状态/关系 | 重复、未知引用、类型、缺参、环 | 不适用 | `ctest -R model` |
| spec | version 1、规范化 | malformed JSON、重复 key、非法版本 | 不适用 | `ctest -R spec` |
| generator | 线性、分支、seed、循环 | case/step=0、order cycle | dead-end/limit | `ctest -R generator` |
| runtime | ok、argument relation | malformed、extra、unknown status | mismatch、error、timeout、crash、cap | `ctest -R runtime` |
| CLI | validate/generate/run/report | invalid spec、reports path | adapter failure、报告写入失败 | `ctest -R cli` |

## 每次变更的验证要求

1. 先运行受影响层级的测试，并记录失败原因。
2. 修改 parser/model/generator/runtime/CLI 后运行完整 Debug CTest。
3. 修改进程、解析或内存相关代码后运行完整 ASan/UBSan/LSan CTest。
4. CLI 行为变更必须运行实际命令并核对退出码、stderr 和报告文件。
5. 失败场景不能用 `|| true` 隐藏；预期失败必须显式比较退出码。

## 尚未覆盖的矩阵

- 完整合法 JSON 的字段重排、嵌套 returns、Unicode 和复杂转义；
- 报告 replay、版本不匹配、哈希篡改；
- 多对象、资源生命周期、guard/mutex/parallel/count/value；
- 报告 step 的 `expected_state`、summary seed 与计数一致性（`cli_verify_report`、`cli_verify_summary`）；
- 覆盖率分母、约束覆盖和未覆盖原因的统一报告。
