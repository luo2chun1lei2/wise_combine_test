# wise-combine-test

`wise-combine` 是一个运行在 Linux 上的命令行组合测试工具。通过 JSON
规范描述对象状态图、带类型的函数、参数传递关系和全局调用顺序关系，生成器
会生成确定且有界的状态迁移流程；运行器通过显式的适配器进程执行每个步骤，
并写出可复现的失败报告。

实现使用 C++20、标准库和 POSIX 进程 API，不依赖运行时第三方模块。

## 文档同步约定

本项目同时维护英文和中文使用文档：`README.md` 与 `README.zh.md`。凡是构建、
CLI、规范、适配器、报告、退出码、安全限制或测量方式发生变化，都必须在同一
次提交中同步修改这两个文件。命令、JSON 字段、限制值和行为保证必须保持一致，
仅说明文字使用不同语言。

## 构建与测试

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
(cd build && ctest --output-on-failure)
```

测试覆盖模型、解析器、生成器、适配器、协议、超时、崩溃、输出上限和 CLI 集成
场景。构建目录和测试二进制文件已被 git 忽略。

## CLI 命令

```sh
./build/wise-combine validate tests/fixtures/state_valid.json
./build/wise-combine generate tests/fixtures/relation_valid.json
./build/wise-combine run tests/fixtures/relation_valid.json \
  --adapter tests/fixtures/bin/adapter_ok \
  --reports reports --run-id relation
./build/wise-combine report reports/relation-0.txt
```

`validate` 和 `generate` 会输出包含墙钟时间、CPU 时间和峰值 RSS 的 JSON。
`run` 会为每条流程写出一对 `<run-id>-N.json` 与 `<run-id>-N.txt` 文件，另外
写出包含 `case_count`、`passed`、`failed` 以及相同测量字段的
`<run-id>-summary.json`。`report` 命令只打印之前生成的报告，不会修改报告文件。

退出码保持稳定：`0` 表示成功，`2` 表示规范格式错误或内容无效，`3` 表示有界
生成耗尽（达到 `case_limit` 或 `step_limit`），`4` 表示适配器观察到不匹配，
`5` 表示超时、崩溃、协议或启动失败，`6` 表示 CLI 用法无效。达到耗尽状态前已
生成的流程仍会被报告并执行；退出码 `3` 表示已达到配置的上限。

## 规范

顶层 JSON 对象包含 `version: 1`、`states`、`initial_state`、`transitions`、
`functions`、`relations`、`limits` 和 `seed` 字段。对象和数组在适用时会按稳定的
字典序规范化。

```json
{
  "version": 1,
  "states": [{"id":"idle"},{"id":"done"}],
  "initial_state": "idle",
  "transitions": [{
    "id":"finish", "from":"idle", "to":"done", "function":"finish",
    "args": {}, "expect": {"state":"done"}
  }],
  "functions": [{"id":"finish","params":[],"returns":[]}],
  "relations": [],
  "limits": {"max_cases": 10, "max_steps": 8, "max_subprocesses": 1},
  "seed": 1
}
```

参数关系必须将一个生产者返回值准确连接到一个消费者参数，并且声明的类型必须
匹配。`before` 关系表示 transition ID 之间的全局顺序边。自环、重复绑定、未知
引用、类型不匹配和顺序环都会被拒绝，并给出诊断指针。`max_cases` 可以为零；
`max_steps` 和 `max_subprocesses` 必须为正数。

## 适配器协议与安全

适配器是一个可执行文件，从标准输入接收一行 JSON：

```json
{"protocol":1,"flow_id":"finish","step":0,"function":"finish","args":{}}
```

它必须向标准输出写出一个 JSON 对象：

```json
{"protocol":1,"status":"ok","observed_state":"done","returns":{},"stderr":""}
```

`status` 可以是 `ok`、`mismatch` 或 `error`；额外 JSON、格式错误的输出、非零退出
以及缺少必需字段都会被视为协议失败。运行器使用不经过 shell 的 `execve`，固定
环境变量（`PATH=/usr/bin:/bin`、`LC_ALL=C`），为每个适配器建立进程组，单步超时
为 2 秒，总超时为 30 秒，标准输出与标准错误合计上限为 16 MiB。只允许执行
`tests/fixtures/bin/` 下名称匹配 `adapter_*` 的可执行文件（或构建出的 fixture
可执行文件）。

## 内存与性能检查

启用 Address、未定义行为和泄漏检查 sanitizer：

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DWISE_COMBINE_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
(cd build-asan && ctest --output-on-failure)
```

安装 Valgrind 后可以额外执行泄漏检查：

```sh
valgrind --leak-check=full --error-exitcode=1 \
  ./build/wise-combine validate tests/fixtures/state_valid.json
```

GNU `/usr/bin/time -v` 可以报告一次完整 CLI 调用的峰值内存和 CPU/墙钟耗时；
CLI 的 JSON 测量字段提供相同的机器可读数据。覆盖率是补充指标：可以使用启用
`--coverage` 的编译器构建，然后在可用时运行 `gcovr` 或 `lcov` 收集结果。
