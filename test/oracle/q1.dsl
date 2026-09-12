types { int -> "int" }
resource Queue { ctype: "int" states: CLOSED, OPEN initial: CLOSED }

func q_open() -> Queue {
  symbol: "q_open"
  signature: "int q_open(void)"
  effects: result -> OPEN
  success: result == 0
}

func q_pop_empty(q: Queue) -> int {
  symbol: "q_pop"
  signature: "int q_pop(int)"
  requires: q is OPEN
  success: result < 0
}
