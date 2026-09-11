# DSL 语法定义（草案 v0.3）

本文件描述函数调用序列模型的 DSL，用于表达函数之间的语义关系：资源生命周期、前置条件、状态变化、跨函数资源依赖，以及真正执行所需的 C 符号、C 类型、成功返回值和取值来源。生成策略（长度、覆盖、负向、算法等）由命令行选项控制，不在模型文件中声明。

## 1. 设计目标

- 语法简单、可读，像模型描述文件而不是编程语言。
- 语义显式声明，不自动推导。
- 函数、状态、符号、类型等描述集中在一个模型里，便于统一检查和发现冲突或不完备。

## 2. 模型文件的组成

一个模型文件由以下块组成，建议按此顺序书写：

- `types`：DSL 值类型到 C 类型的映射。
- `values`：取值来源，即值的范围或列表。
- `resource`：资源类型。
- `func`：函数。
- `setup`：预创建资源实例（可选）。

以 `#` 开始的行是注释。

## 3. types：值类型映射

```
types {
  string -> "const char*"
  int -> "int"
  bool -> "int"
}
```

把 DSL 中使用的值类型映射到生成 harness 时使用的 C 类型。除内建 `string`、`int`、`bool` 外，也可以声明自定义类型名，例如 `usize`、`buffer`；只要该类型名出现在本块中，函数参数或返回值就可以使用它。

## 4. values：取值来源

```
values {
  paths: ["a.txt", "b.txt", "c.txt"]
  datas: ["hello", "world"]
  sizes: 1..100
}
```

- `名称: [值1, 值2, ...]` 表示列表。
- `名称: 下限..上限` 表示整数范围。
- 执行时从来源中随机选择一个值；随机结果由随机种子控制，保证可复现。
- 当前默认生成算法会枚举长度 1 到 `--max-length` 的合法调用序列；使用 `--algorithm random` 时按随机游走生成。

## 5. resource：资源类型

```
resource FileHandle {
  ctype: "int"
  states: CLOSED, OPEN
  initial: CLOSED
}
```

- `ctype`：该资源类型在 C 中的类型。
- `states`：资源的所有状态。
- `initial`：资源的初始状态。
- `observe`：可选，一个返回状态名字符串的 C 函数，用于在每次状态变化后校验实际状态。

## 6. func：函数

```
func 函数名(参数, ...) -> 返回类型 {
  symbol: "C 函数符号"
  signature: "完整 C 函数原型"
  requires: [条件, ...]
  effects: [效果, ...]
  success: [返回值条件]
}
```

- `symbol`：真实 C 函数符号名。
- `signature`：完整 C 函数原型，用于生成调用代码。
- `requires`：调用前必须满足的资源状态条件，为空表示无条件。
- `effects`：调用后资源状态的变化，为空表示所有资源状态不变。
- `success`：调用后对原始 C 返回值的判定；不满足即判定该序列失败并终止。可以写 `true`、`false`，也可以写比较表达式。

参数形式为 `名称: 类型`，值参数可以附加取值来源：`名称: 类型 from 来源`。输出缓冲区参数写成 `out 名称: 类型`，表示该参数由函数写入，生成桩代码时会为其分配可写缓冲区。

## 7. 条件、效果与成功判定

- 条件形式：`<参数名> is <状态>`，多个条件用逗号分隔。
- 效果形式：`<参数名> -> <状态>` 或 `result -> <状态>`，多个效果用逗号分隔。
- 成功判定形式：`true`、`false`，或对 `result`/参数/常量/NULL 的比较表达式，例如 `result == 0`、`result >= 0`、`result != NULL`、`result == nmemb`；多个比较可用 `&&`、`||` 和括号组合。
- `result` 是关键字：在 `effects` 中表示返回值对应的资源实例；在 `success` 中表示原始 C 返回值。

## 8. 语义

### 8.1 生产者与消费者

- 返回资源类型的函数是**生产者**，每调用一次产生一个新的资源实例。
- 参数为资源类型的函数是**消费者**，参数必须绑定到一个已存在的资源实例。

### 8.2 资源实例环境

生成器维护一个实例环境，记录每个已产生实例及其当前状态。调用函数时，每个资源参数都要从环境里选一个当前状态满足 `requires` 的实例来绑定。

### 8.3 合法调用序列

从空环境开始，每一步都满足 `requires` 且资源绑定有效的调用序列，称为合法调用序列。

### 8.4 随机取值

- 值参数从对应的 `values` 来源中随机选择一个值。
- 资源参数在环境里满足 `requires` 的实例中随机选择一个；同一函数的不同资源参数不会绑定到同一个实例。

### 8.5 预创建实例（setup）

可以用 `setup` 块在生成前预创建资源实例：

```
setup {
  名称: 生产者函数(参数, ...) x 次数
}
```

例如 `handles: open("a.txt") x 3` 会在环境里先放入 3 个由 `open` 产生的句柄。

## 9. 完整示例：文件句柄

```
types {
  string -> "const char*"
  int -> "int"
  bool -> "int"
}

values {
  paths: ["a.txt", "b.txt", "c.txt"]
  datas: ["hello", "world"]
}

resource FileHandle {
  ctype: "int"
  states: CLOSED, OPEN
  initial: CLOSED
}

func open(path: string from paths) -> FileHandle {
  symbol: "open"
  signature: "int open(const char *)"
  requires:
  effects: result -> OPEN
  success: result >= 0
}

func write(h: FileHandle, data: string from datas) -> int {
  symbol: "write"
  signature: "int write(int, const char *)"
  requires: h is OPEN
  effects: h -> OPEN
  success: result == 0
}

func close(h: FileHandle) -> int {
  symbol: "close"
  signature: "int close(int)"
  requires: h is OPEN
  effects: h -> CLOSED
  success: result == 0
}
```

## 10. 生成与执行选项

模型文件只描述语义，生成和执行策略通过命令行控制：

- `--max-length N`：生成长度 1 到 N 的序列，默认 3。
- `--seed N`：随机种子，默认 0。
- `--max-cases N`：最大用例数上限；`--algorithm random` 时同时作为尝试次数。
- `--negative`：追加负向序列。
- `--coverage`：输出函数覆盖统计。
- `--cover`：用贪心算法求覆盖全部函数的最小序列集。
- `--algorithm dfs|bfs|random`：生成算法；`random` 用随机游走。
- `--bind enumerate|random`：资源实例绑定策略，默认枚举全部合法绑定。
- `--json`：输出 JSON 报告。
- `--harness`：生成并输出 C harness 代码。
- `--harness-json`：harness 失败时输出结构化 JSON。
- `--dylib`：生成需要动态加载被测库的 C harness 代码。
- `--replay N`：输出第 N 个合法序列的 harness，便于复现。


## 11. 相关决定

本语法的各项决定记录在 `ai/adr.md`，尤其是 ADR-002、ADR-008、ADR-009、ADR-010、ADR-011、ADR-012。

## 12. C++ 类方法映射

函数除了映射到自由 C 函数，也可以映射到 C++ 类的成员方法。先声明类，再在函数里用 `receiver` 指定所属类：

```
class Store {
  cpp: "Store"
  header: "Store.h"
}

func put(data: string) -> int {
  receiver: Store
  symbol: "put"
  signature: "int put(const char*)"
  requires:
  effects:
  success: result == 0
}
```

- `class` 声明 C++ 类：`cpp` 是类名，`header` 是包含该类声明的头文件。
- `receiver` 表示该函数是 `Store` 的成员方法，`signature` 只写返回值、方法名和显式参数，不包含 `this`。
- 生成 harness 时，会 `#include` 对应头文件，并在每个测试函数中创建一个默认构造的局部对象，调用形如 `对象.方法(参数)`。
- 含 C++ 类的模型生成的是 C++ harness，需用 `g++` 编译；动态库加载模式暂不支持成员方法。
