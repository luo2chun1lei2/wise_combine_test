# 连接状态机示例
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
