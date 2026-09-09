# 内存检查报告

## 检查对象

- 工具本体 `src/out/wise_combine_test` 与 `src/out/wise_combine_test_asan`。
- 单元测试 `test/out/test_wise_asan`。
- 独立被测程序 `wise_standalone_asan`（由 `--mode standalone` 生成）。

## 方法

使用 AddressSanitizer（ASan）编译并运行，检查内存泄露与内存越界访问：

```text
make -C src asan
make -C test asan
```

运行时启用 LeakSanitizer（`ASAN_OPTIONS=detect_leaks=1`）进行内存泄露与越界检查。

## 结果

- `src/out/wise_combine_test_asan` 解析示例并 `--dry-run`：退出码 0，无 ASan / LeakSanitizer 报错。
- `test/out/test_wise_asan`：全部单元测试通过，退出码 0，无 ASan / LeakSanitizer 报错。
- `wise_standalone_asan`（链接示例库）：组合流程按预置问题返回非零，无 ASan / LeakSanitizer 报错。

Valgrind 在当前环境未安装，未能执行 Valgrind 检查；ASan 与 LeakSanitizer 已覆盖内存泄露与越界访问。工具及生成代码未使用手动 `new`/`delete`，资源由 RAII 与标准库容器管理。
