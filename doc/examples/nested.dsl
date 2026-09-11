# 复合状态示例
machine Device {
  states: OFF, ON, ON_IDLE, ON_WORKING
  initial: OFF
  events: power_on, power_off, start, stop

  state ON {
    initial: ON_IDLE
    state ON_IDLE {
    }
    state ON_WORKING {
    }
  }

  transition OFF -> ON on power_on {
  }
  transition ON -> OFF on power_off {
  }
  transition ON_IDLE -> ON_WORKING on start {
  }
  transition ON_WORKING -> ON_IDLE on stop {
  }
}
