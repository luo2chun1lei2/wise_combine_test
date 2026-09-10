# 状态机 DSL 语法定义（草案 v0.1）

本文件描述状态机模型（目标二）的 DSL，用于表达状态、事件、转换、guard 条件，以及进入和离开状态时的动作。

## 1. 设计目标

- 语法简单、可读，像状态图描述文件而不是编程语言。
- 支持扁平有限状态机：状态、事件、转换、guard 条件、entry/exit 动作。
- 后续预留升级到嵌套、复合、历史和并发状态的能力。

## 2. 模型文件的组成

一个状态机模型由 `machine` 块描述，其中包含：

- 状态列表和初始状态。
- 事件列表。
- 可选的 `state` 块，用于声明 entry/exit 动作。
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
}
```

- `entry` 是进入该状态时执行的动作。
- `exit` 是离开该状态时执行的动作。
- 两者都可以省略。

### 3.3 transition 声明

```
transition 源状态 -> 目标状态 on 事件 {
  guard: [条件表达式]
  action: [动作]
}
```

- `源状态` 和 `目标状态` 必须是已声明的状态。
- `事件` 是触发该转换的事件。
- `guard` 是可选条件，只有为真时转换才发生。
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

## 6. 待细化

- `guard` 条件表达式的具体语言，以及是否引用事件携带的数据或返回值。
- `entry`、`exit`、`action` 动作的执行语义，以及是否与函数模型或被测接口对接。
- 同一状态、同一事件存在多个 guard 时如何选择转换。
- 后续升级到嵌套、复合、历史和并发状态的语法预留。

## 7. 相关决定

本语法的范围由 `ai/adr.md` 的 ADR-014 确定：第一版支持扁平 FSM、guard 和 entry/exit 动作，后续升级到嵌套、复合、历史和并发状态。
