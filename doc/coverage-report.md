# 代码覆盖率报告

## 方法

使用 GCC `--coverage` 编译单元测试并运行，再用 `gcov` 统计 `src/wise.cpp` 的覆盖率：

```text
make -C test coverage
./test/out/test_wise_cov
cd test && gcov -b -c out/wise_cov.gcno out/test_wise_cov.gcno
```

## 结果

| 指标 | 数值 |
| --- | ---: |
| 行覆盖率 | 82.31% |
| 分支覆盖率 | 80.48% |
| 分支至少执行一次 | 51.21% |
| 调用覆盖率 | 73.29% |

行覆盖率与分支覆盖率均达到 80% 的可选目标。
