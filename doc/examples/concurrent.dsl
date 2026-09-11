# 并发状态示例
machine System {
  states: OFF, ON, REGION_A, REGION_B, A1, A2, B1, B2
  initial: OFF
  events: power_on, power_off, a_next, b_next

  state ON {
    concurrent
    state REGION_A {
      initial: A1
      state A1 {
      }
      state A2 {
      }
    }
    state REGION_B {
      initial: B1
      state B1 {
      }
      state B2 {
      }
    }
  }

  transition OFF -> ON on power_on {
  }
  transition ON -> OFF on power_off {
  }
  transition A1 -> A2 on a_next {
  }
  transition A2 -> A1 on a_next {
  }
  transition B1 -> B2 on b_next {
  }
  transition B2 -> B1 on b_next {
  }
}
