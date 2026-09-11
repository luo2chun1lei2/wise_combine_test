# 状态机动作映射到 C++ 类方法
machine M {
  states: S0, S1
  initial: S0
  events: go, back

  class Worker {
    cpp: "Worker"
    header: "Worker.h"
  }

  actions {
    on_enter: Worker.begin
    on_exit: Worker.end
    do_go: Worker.go
  }

  state S0 {
    exit: on_exit
  }
  state S1 {
    entry: on_enter
  }

  transition S0 -> S1 on go {
    action: do_go
  }
  transition S1 -> S0 on back {
  }
}
