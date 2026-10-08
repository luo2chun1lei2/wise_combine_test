# wise_combine_test

`wise-combine-test` 是一个运行在 Linux 上的小型 C11 组合测试运行器。模型文件
是纯文本，包含带版本的 schema、状态迁移和函数关系。公共 C API 声明在
`include/wct.h` 中。

## 构建和运行

```sh
make
./bin/wise-combine-test --help
./bin/wise-combine-test --model fixtures/smoke.model --mode state
./bin/wise-combine-test --model fixtures/relation.model --mode relation
# 捕获并重放一次确定性执行
./bin/wise-combine-test --model fixtures/function_relations.model --mode relation \
  --trace /tmp/function.trace
./bin/wise-combine-test --replay /tmp/function.trace
```

DSL 使用空白字符分隔：

```text
schema 1
state_graph graph-id initial-state
state state-id
transition transition-id from-state to-state input expected-output
relation_graph calls-id
call function-id argument...
relation prerequisite dependent
contract function-id argc type...
```

`schema 1` 是磁盘格式的兼容边界。新指令和 contract 属性只能以追加方式扩展；
旧读取器必须拒绝未知的必需指令，而不能静默改变执行行为。trace 文件自带
`WCT_TRACE 1` 版本，以及规范的 model/IR/metadata digest。只有 schema 和所有
已记录输入都匹配时，replay 才会被接受。

call contract 是可选的。例如，`contract fetch 2 string int` 要求两个参数，并
校验它们声明的字面量类型（`int`、`bool`、`string`、`bytes`、`ref` 或 `any`）。

contract 可以追加 `result=<type>` 来校验回调结果，也可以追加 `expect=<text>`
来要求结果完全匹配。producer 声明的结果类型还会约束 `$producer` 引用参数。
这两个属性都是可选的，因此既有 schema-1 contract 仍然有效。

`--mode state` 按有效图顺序执行可达迁移。分支场景可能从初始状态重放前缀；
调用方拥有可变状态时，应在场景重放前提供 `wct_limits.state_reset`。
`--mode relation` 按依赖顺序执行调用，并输出有序调用轨迹。`$call` 参数是隐式
依赖，会接收 producer 回调的结果。`--trace` 写入带版本和校验和的文本 trace；
`--replay` 重新运行 trace 引用的模型，并拒绝模型、步数、退出码或 trace digest
发生变化的情况。生产用户应通过 C API 提供回调，而不应依赖这些示例回调。无效
模型和回调失败返回退出码 1；CLI 用法错误返回退出码 2。

C API 和 CLI 默认将每个完整场景隔离到 POSIX 子进程中，包括 trace 捕获和
replay。`--isolate` 只是为兼容性保留，不会添加第二层隔离边界。`--timeout-ms N`
独立于该标志，用于显式设置整个场景的单调时钟截止时间。隔离状态会包含在报告
中（`process_exit`、`process_signal`、`timed_out`）。trace 捕获会写入无缓冲的
子进程输出，并根据 trace 文件重新计算步骤 digest，因此回调副作用保持隔离，
replay 也保持确定性。

trace digest 是无密钥的 64 位一致性校验值。replay 能发现对已记录字段的意外
修改和确定性篡改；它不能认证 trace，也不能防御重写所有字段和 digest 的攻击者。
replay 会按 trace 中记录的模型路径打开文件，因此只有在信任该路径时才应重放
trace。超时设置是执行控制项，不是已记录的 replay 输入；重放场景没有捕获原始
截止时间。

状态隔离是事务性的，并且以场景为范围：父进程拥有状态快照；状态子进程成功后
提交其序列化后状态。失败场景、失败回调、超时、断言不匹配和最终提交失败都会
原子回滚。relation 场景完全在子进程中执行，并且有意不提交任意回调上下文；
调用方必须通过回调 contract 返回结果。子进程在单调时钟截止时间内执行回调。
只有状态场景的结果满足迁移 contract 后，序列化状态才会提交。失败回调、超时
和断言不匹配都会被丢弃。可变 API 上下文必须提供成对的 snapshot/restore 钩子；
分支重放使用 reset 钩子，从声明的初始状态开始每个场景。

## 验证

`make test` 运行独立冒烟检查和受控 queue-oracle 回归
（`tests/test_queue_oracle.sh`）：包含 84 次 clean/mutant 矩阵运行，以及
cycle/subset/repeat 探针。`make sanitize` 启用 AddressSanitizer 和
UndefinedBehaviorSanitizer，并运行隔离的故意越界/泄漏哨兵用例；预期中的
sanitizer 发现会与产品失败分开分类。`make valgrind` 在 Valgrind 已安装时运行，
否则记录显式跳过。可以使用
`LC_ALL=C make measure OUT=evidence/iter-0/measure.tsv` 生成测量 TSV。默认重复
固定命令 3 次，记录 wall/user/system CPU 时间和最大 RSS，并写入配套的
median/min/max/range 表。受控比较可以使用 `REPEAT=N`、`FIXTURE=...` 和
`MODE=...`。

迭代门禁和验证证据记录在
[`docs/release-readiness.md`](docs/release-readiness.md)。`make coverage` 清理
过期的 `*.gcda`、`*.gcno` 和报告文件，为 CLI 和 API 测试框架共同使用的共享库
目标文件启用插桩，运行 CLI/API/fuzz/queue-oracle 套件，并为发现的每个 profile
生成一份 gcov 报告，同时写入 `coverage/summary.txt`。如果缺少预期的
`src/wct.c` 和 `tools/wct_cli.c` profile，该目标会失败。

最近一次本地覆盖率测量使用 Linux 上的 GCC/gcov 9.4.0：

| 源码 | 已执行行 | 已执行分支 | 至少执行一次的分支 |
| --- | ---: | ---: | ---: |
| `src/wct.c` | 78.55%（718 行） | 81.61%（1142 个分支） | 58.41% |
| `tools/wct_cli.c` | 92.01%（338 行） | 96.45%（620 个分支） | 67.10% |

`make coverage` 导出 `WCT_COVERAGE_ROOT`。每个正常 fork 退出都会调用
`child_exit()`；当存在 coverage root 时，它会将 GCC 的 `GCOV_PREFIX` 设置为
按子进程 PID 区分的目录，然后在 `_exit` 前调用 `__gcov_dump`。这样可以避免
fork coverage 常见的丢失问题：父进程稍后写 profile 时覆盖子进程的数据。
`tools/merge-coverage.sh` 会把 profile 过滤到 `build/` 中存在的目标文件，使用
GCC 的 `gcov-tool` 合并，并在 `gcov` 生成报告前把合并后的产品 profile 复制回
原位置。测试框架自身的 `test_api` profile 有意排除，因为发布测量目标是
`src/wct.c` 和 `tools/wct_cli.c`。如果子进程会调用 `exec`，则必须在替换进程映像
前 dump profile；本运行器不会用 `exec` 替换其 fork 出的子进程。

上面的测量数字是当前基线，不是“覆盖率已达 80%”的声明：库代码行覆盖率仍低于
80%，两项分支命中 totals 也都低于 80%。Valgrind 在运行环境中是可选的；
未安装时，`make valgrind` 会记录显式跳过。
