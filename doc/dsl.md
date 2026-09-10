# DSL 语法定义（草案 v0.2）

本文件描述函数调用序列模型的 DSL，用于表达函数之间的语义关系：资源生命周期、前置条件、状态变化、跨函数资源依赖，以及真正执行所需的 C 符号、C 类型、成功返回值和取值来源。

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

以 `#` 开始的行是注释。

## 3. types：值类型映射

```
types {
  string -> "const char*"
  int -> "int"
  bool -> "int"
}
```

把 DSL 内建值类型映射到生成 harness 时使用的 C 类型。

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
- `success`：调用后对原始 C 返回值的判定；不满足即判定该序列失败并终止。

参数形式为 `名称: 类型`，值参数可以附加取值来源：`名称: 类型 from 来源`。

## 7. 条件、效果与成功判定

- 条件形式：`<参数名> is <状态>`，多个条件用逗号分隔。
- 效果形式：`<参数名> -> <状态>` 或 `result -> <状态>`，多个效果用逗号分隔。
- 成功判定形式：对 `result` 的比较表达式，例如 `result == 0`、`result >= 0`。
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
- 资源参数在环境里满足 `requires` 的实例中随机选择一个。
- 若需要预先创建多个资源实例再随机选择，可以先多次调用生产者函数生成实例列表；该机制的具体写法待细化。

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

## 10. 待细化

- 资源参数“预先创建多个实例再随机选择”的精确语法。
- `success` 表达式是否需要支持参数比较、逻辑组合等更复杂形式。
- 参数别名、负向序列、序列长度上限等生成策略的声明方式。

## 11. 相关决定

本语法的各项决定记录在 `ai/adr.md`，尤其是 ADR-002、ADR-008、ADR-009、ADR-010、ADR-011、ADR-012。
