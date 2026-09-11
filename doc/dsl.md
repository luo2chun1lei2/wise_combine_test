# DSL 语法定义

组合测试工具使用自定义文本格式描述对象状态图和函数关系。本文定义该格式的语法，示例文件见 `examples/`。

## 1. 基本约定

- 以 `#` 开头的行是注释。
- 语句按行书写，使用花括号 `{}` 表示块。
- 状态名、函数名和参数名使用标识符。

## 2. 状态图描述

语法：

```text
object <对象名> {
  state <状态名> [initial] [final]
  transition <源状态> -> <目标状态> by <函数名> [expect <整数>] [guard return <比较符> <整数>]
}
```

字段说明：

- `state`：声明一个状态。`initial` 表示初始状态，`final` 表示终止状态。
- `transition`：声明一条状态迁移。`by` 后为触发迁移的函数；`expect <整数>` 声明预期返回值；`guard` 为可选守卫条件，当前守卫只支持 `return <比较符> <整数>`，比较符可以是 `==`、`!=`、`<`、`<=`、`>`、`>=`。

示例见 [examples/connection.ct](examples/connection.ct)。

## 3. 函数关系描述

语法：

```text
function <函数名>(<参数列表>) [-> <返回参数>]
parameter <函数名>.<参数名> = <函数名>.<参数名> | <常量>
order <前驱函数> before <后继函数>
mutex <函数名> <函数名> [<函数名> ...]
constraint count(<函数名>) <比较符> <整数> [and count(<函数名>) <比较符> <整数> ...]
constraint state <状态名>
constraint value(<函数名>.<参数名>) ==|!= <字面量>
```

字段说明：

- `function`：声明被测函数及其参数、返回参数。
- `parameter`：声明参数之间的数据传递关系，右值可以是另一个函数的参数/返回值，也可以是常量（例如 `parameter start.config = "default"`）。
- `order`：声明调用顺序约束，前驱函数必须先于后继函数调用。
- `mutex`：声明一组函数互斥，同一组合调用流程中至多出现其中一个。
- `constraint`：声明组合约束，支持一个或多个 `count(<函数名>) <比较符> <整数>` 用 `and` 连接；比较符可以是 `<`、`<=`、`>`、`>=`、`==`、`!=`；支持 `constraint state <状态名>` 限制状态路径最终状态；支持 `constraint value(<函数名>.<参数名>) ==|!= <字面量>` 校验参数常量值。

示例见 [examples/functions.ct](examples/functions.ct)。

## 4. 约束

- 迁移引用的源状态和目标状态必须已声明。
- 迁移引用的函数必须在函数关系中已声明。
- 迁移上的 `guard` 必须符合 `return <比较符> <整数>` 格式。
- 参数传递两端的参数类型必须兼容。
- `parameter` 的数据依赖不能形成循环。
- `order` 不能形成循环依赖。
- `mutex` 引用的函数必须已声明，且同一组内不能重复。
- `constraint` 表达式中的函数必须已声明。
- `constraint state` 引用的状态必须已声明。
- `constraint value` 引用的函数和参数必须已声明。
- 必须存在且仅存在一个初始状态。
