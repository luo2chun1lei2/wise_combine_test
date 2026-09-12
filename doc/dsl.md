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
  transition <源状态> -> <目标状态> by <函数名> [expect <整数>] [expect_output "<文本>"] [guard return <比较符> <整数>]
}
```

字段说明：

- `state`：声明一个状态。`initial` 表示初始状态，`final` 表示终止状态。
- `transition`：声明一条状态迁移。`by` 后为触发迁移的函数；`expect <整数>` 声明预期返回值；`expect_output "<文本>"` 声明预期标准输出；`guard` 为可选守卫条件，当前守卫只支持 `return <比较符> <整数>`，比较符可以是 `==`、`!=`、`<`、`<=`、`>`、`>=`。

示例见 [examples/connection.ct](examples/connection.ct)。

## 3. 函数关系描述

语法：

```text
function <函数名>(<参数列表>) [-> <返回参数>]
parameter <函数名>.<参数名> = <函数名>.<参数名> | <常量>
order <前驱函数> before <后继函数> | order <后继函数> after <前驱函数> [if <函数名>]
mutex <函数名> <函数名> [<函数名> ...]
parallel <函数名> <函数名>
constraint count(<函数名>) <比较符> <整数> [and count(<函数名>) <比较符> <整数> ...]
constraint state <状态名>
constraint value(<函数名>.<参数名>) ==|!= <字面量>
```

字段说明：

- `function`：声明被测函数及其参数、返回参数。
- `parameter`：声明参数之间的数据传递关系，右值可以是另一个函数的参数/返回值，也可以是常量（例如 `parameter start.config = "default"`）。
- `order`：声明调用顺序约束，前驱函数必须先于后继函数调用；`order a before b` 与 `order b after a` 等价；可附加 `if <函数名>` 表示仅当该函数出现在流程中时才应用。
- `mutex`：声明一组函数互斥，同一组合调用流程中至多出现其中一个。
- `parallel`：声明两个函数必须在同一状态路径中同时出现或同时不出现。
- `constraint`：声明组合约束，支持一个或多个 `count(<函数名>) <比较符> <整数>` 用 `and` 连接；比较符可以是 `<`、`<=`、`>`、`>=`、`==`、`!=`；支持 `constraint state <状态名>` 限制状态路径最终状态；支持 `constraint value(<函数名>.<参数名>) ==|!= <字面量>` 校验参数常量值。

示例见 [examples/functions.ct](examples/functions.ct)。

## 4. 约束

- 迁移引用的源状态和目标状态必须已声明。
- 迁移引用的函数必须在函数关系中已声明。
- 迁移上的 `guard` 必须符合 `return <比较符> <整数>` 格式。
- 每个函数参数必须有且仅有一个来源：`parameter` 常量、前序函数参数/返回值，或显式外部输入（当前以 `parameter` 常量表示外部输入）。
- 参数传递两端的参数类型必须兼容。
- `parameter` 的数据依赖不能形成循环。
- `order` 不能形成循环依赖。
- `mutex` 引用的函数必须已声明，且同一组内不能重复。
- `parallel` 引用的函数必须已声明，且不能引用同一个函数。
- `constraint` 表达式中的函数必须已声明。
- `constraint state` 引用的状态必须已声明。
- `constraint value` 引用的函数和参数必须已声明。
- 必须存在且仅存在一个初始状态。

## 5. 执行模式与参数

- `direct` 和 `standalone` 模式只支持无参 C ABI 函数；出现带参函数时会明确拒绝。
- 需要参数传递、返回值传递或 `expect_output` 时，使用 `--adapter` 模式。
- adapter 请求包含 `protocol`、`flow_id`、`step`、`function`、`return_type`、`args` 和 `arg_types`；响应是版本为 1 的 JSON 对象，必须包含 `protocol`、`status`、`returns`、`stdout`，可选 `return`。
- 函数组合生成器默认生成空流程、子集、符合 `order` 的排列以及每个函数至多出现 2 次的有界重复调用；可用 `--max-function-repeats` 调整重复上限。
