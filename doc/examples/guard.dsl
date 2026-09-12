# 带 guard 的状态机示例
machine Guarded {
  states: A, B, C, D
  initial: A
  events: go, ok

  transition A -> B on go {
  }

  transition B -> C on ok {
    guard: result == OK
  }

  transition B -> D on ok {
    guard: result == FAIL
  }
}
