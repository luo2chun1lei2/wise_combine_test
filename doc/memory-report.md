# 内存检查报告

## 检查对象

- 工具本体 `src/wise_combine_test` 与 `src/wise_combine_test_asan`。
- 单元测试 `test/test_wise_asan`。
- 独立被测程序 `wise_standalone_asan`（由 `--mode standalone` 生成）。

## 方法

使用 AddressSanitizer（ASan）编译并运行，检查内存泄露与内存越界访问：

```text
make -C src asan
make -C test asan
```

运行环境为受限沙箱，LeakSanitizer 因 ptrace 限制无法启用，因此运行时设置 `ASAN_OPTIONS=detect_leaks=0`，但仍启用地址越界、释放后使用等检查。

## 结果

- `src/wise_combine_test_asan` 解析示例并 `--dry-run`：退出码 0，无 ASan 报错。
- `test/test_wise_asan`：全部单元测试通过，退出码 0，无 ASan 报错。
- `wise_standalone_asan`（链接示例库）：组合流程按预置问题返回非零，无 ASan 报错。

Valgrind 在当前环境未安装，未能执行 Valgrind 检查。工具及生成代码未使用手动 `new`/`delete`，资源由 RAII 与标准库容器管理，内存越界风险通过单元测试与 ASan 覆盖。
