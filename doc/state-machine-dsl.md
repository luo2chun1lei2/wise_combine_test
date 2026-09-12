# 状态机 DSL 语法定义（草案 v0.2）

本文件描述状态机模型（目标二）的 DSL，用于表达状态、事件、转换、guard 条件，以及进入和离开状态时的动作。

## 1. 设计目标

- 语法简单、可读，像状态图描述文件而不是编程语言。
- 支持状态、事件、转换、guard 条件、entry/exit 动作。
- 支持嵌套、复合、浅历史和并发状态。

## 2. 模型文件的组成

一个状态机模型由 `machine` 块描述，其中包含：

- 状态列表和初始状态。
- 事件列表。
- 可选的 `state` 块，用于声明 entry/exit 动作，也可以嵌套。
- 一个或多个 `transition` 声明。

以 `#` 开始的行是注释。

## 3. 语法

### 3.1 machine 声明

```
machine 名称 {
  states: 状态1, 状态2, ...
  initial: 初始状态
  events: 事件1, 事件2, ...
}
```

### 3.2 state 块

```
state 状态名 {
  entry: [动作]
  exit: [动作]
  initial: [子状态]
  history: [伪状态名]
  history*: [伪状态名]
  concurrent
  state 子状态 { ... }
}
```

- `entry` 是进入该状态时执行的动作。
- `exit` 是离开该状态时执行的动作。
- `initial` 用于复合状态，声明进入时激活的初始子状态。
- `history` 声明浅历史伪状态；`history*` 声明深历史伪状态。
- `concurrent` 表示并发状态，其直接子状态作为正交区域并行激活。
- `state` 可以嵌套，形成复合状态。
- 这些字段都可以省略。

### 3.3 transition 声明

```
transition 源状态 -> 目标状态 on 事件 {
  guard: [条件表达式]
  action: [动作]
}
```

- `源状态` 和 `目标状态` 必须是已声明的状态。
- `事件` 是触发该转换的事件。
- `guard` 是可选条件。路径生成、`--events` 执行和生成的 harness 会使用同一套求值规则；guard 中引用的变量需要通过 `--guard 名称=值` 绑定。
- `action` 是转换发生时执行的可选动作。

## 4. 语义

- 状态机是一个有限状态机，节点是状态，边是转换。
- 转换触发条件：当前状态等于 `源状态`，收到 `事件`，且 `guard` 为真。
- 发生转换时依次执行：离开源状态的 `exit` 动作、转换的 `action` 动作、进入目标状态的 `entry` 动作。
- 一条路径是从初始状态出发，经过一系列转换形成的序列。

## 5. 完整示例：连接状态机

```
machine Connection {
  states: DISCONNECTED, CONNECTING, CONNECTED, CLOSED
  initial: DISCONNECTED
  events: connect, connected_ok, connected_fail, disconnect, close

  state CONNECTED {
    entry: start_timer
    exit: stop_timer
  }

  transition DISCONNECTED -> CONNECTING on connect {
    action: reset_retry
  }

  transition CONNECTING -> CONNECTED on connected_ok {
    guard: result == OK
  }

  transition CONNECTING -> DISCONNECTED on connected_fail {
  }

  transition CONNECTED -> DISCONNECTED on disconnect {
  }

  transition DISCONNECTED -> CLOSED on close {
  }
}
```

## 6. 当前实现与限制

- `guard` 支持形如 `result == OK` 的比较表达式，也支持 `&&`、`||` 和括号组合；可用 `--guard 名称=值` 绑定变量。右操作数是未绑定标识符时按字面量处理；左操作数变量未绑定时会报错，不会再静默视为成立。
- `entry`、`exit`、`action` 在无映射时，`--events` 执行仅作为动作轨迹输出；若用 `class` 和 `actions` 声明映射到 C++ 类方法，配合 `--events ... --harness` 会生成 C++ 驱动并真正调用这些方法。
- 路径生成算法默认 DFS 枚举长度 1 到 `--max-length` 的路径；支持 `--algorithm dfs|bfs|random|tour`。
- `--events e1,e2,...` 按事件序列执行并输出最终状态；若同一状态和事件存在多个转换，当前选择第一条匹配转换。
- `--coverage` 输出状态覆盖和转换覆盖统计；`--n-switch N` 输出 N-switch 覆盖；`--cover` 用贪心算法求覆盖全部转换的最小路径集。
- 并发状态在路径生成和 `--events` 执行中均以活动叶子状态集合建模。
- 路径生成会求值 guard：guard 为空或满足时才走该转换；guard 变量未绑定时跳过该转换，并在结果中报告 `skipped_guards`。`--events` 执行时未绑定 guard 变量会返回非零退出码。

## 7. 相关决定

本语法的范围由 `ai/adr.md` 的 ADR-014 确定：第一版支持扁平 FSM、guard 和 entry/exit 动作，后续升级到嵌套、复合、历史和并发状态。
