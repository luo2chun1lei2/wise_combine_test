# 浅历史与深历史对比示例
machine Device {
  states: OFF, ON, REGION, SUB1, SUB2, SUB1_A, SUB1_B
  initial: OFF
  events: power_on, power_off, to_b, resume_shallow, resume_deep

  state ON {
    initial: REGION
    history: SH
    history*: DH
    state REGION {
      initial: SUB1
      state SUB1 {
        initial: SUB1_A
        state SUB1_A {
        }
        state SUB1_B {
        }
      }
      state SUB2 {
      }
    }
  }

  transition OFF -> ON on power_on {
  }
  transition ON -> OFF on power_off {
  }
  transition SUB1_A -> SUB1_B on to_b {
  }
  transition OFF -> SH on resume_shallow {
  }
  transition OFF -> DH on resume_deep {
  }
}
