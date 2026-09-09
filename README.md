# wise_combine_test 组合测试工具

组合测试工具用于在描述接口之间的关系后，自动生成并执行各种组合调用流程，检测对象状态迁移和函数组合调用中的问题。

## 目录结构

- `ai/`：AI 与项目相关文件，包括项目提案、设计文档和任务计划。
- `src/`：实现代码与编译构建脚本。
- `test/`：测试代码与编译构建脚本。
- `doc/`：面向客户的文档，包括 DSL 语法定义和示例文件。
- `README.md`：本文档。

## 编译

在项目根目录执行 `make` 可一次性编译工具、测试和示例库：

```text
make
```

也可以只构建工具源码：

```text
make -C src
```

生成的工具位于 `src/out/wise_combine_test`。

## 验证

在项目根目录执行：

```text
make check
```

该命令会编译整个系统、运行单元测试、执行示例组合测试，并生成独立被测程序。也可以单独构建和运行测试：

```text
make -C test
./test/out/test_wise
```

运行地址消毒器检查：

```text
make asan
```

清理所有生成物：

```text
make clean
```

单独生成并编译独立被测程序：

```text
make standalone
```

生成并用 ASan 编译运行独立被测程序：

```text
make standalone-asan
```

## 安装

构建后，将 `src/out/wise_combine_test` 复制到 `PATH` 中的目录即可：

```text
install -m 0755 src/out/wise_combine_test /usr/local/bin/wise_combine_test
```

## 使用

1. 编写状态图与函数关系描述文件，DSL 语法定义见 [doc/dsl.md](doc/dsl.md)。
2. 参考示例文件，位于 [doc/examples/](doc/examples/)。
3. 运行组合测试：

```text
wise_combine_test <描述文件> [选项]
```

例如，使用提供的示例描述和示例动态库：

```text
make -C doc/examples
./src/out/wise_combine_test doc/examples/connection.ct doc/examples/functions.ct \
  --lib doc/examples/libconn.so
```

只生成组合流程、不执行时使用 `--dry-run`；生成独立被测程序时使用 `--mode standalone`。完整命令选项可用 `wise_combine_test --help` 查看，DSL 语法见 [doc/dsl.md](doc/dsl.md)。

### 独立被测程序

生成独立被测程序：

```text
./src/out/wise_combine_test doc/examples/connection.ct doc/examples/functions.ct \
  --mode standalone --lib doc/examples/libconn.so
```

该命令会生成 `build/wise_standalone.cpp` 并打印编译命令。若链接示例库，可执行：

```text
g++ -std=c++17 build/wise_standalone.cpp -Ldoc/examples -lconn \
  -Wl,-rpath,'$ORIGIN/../doc/examples' -o build/wise_standalone
./build/wise_standalone
```

用 ASan 编译运行：

```text
g++ -std=c++17 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
  build/wise_standalone.cpp -Ldoc/examples -lconn \
  -Wl,-rpath,'$ORIGIN/../doc/examples' -o build/wise_standalone_asan
ASAN_OPTIONS=detect_leaks=1 ./build/wise_standalone_asan
```

详细文档见 `doc/`。
