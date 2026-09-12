types { int -> "int" }
resource Queue { ctype: "int" states: CLOSED, OPEN initial: CLOSED }

func q_open() -> Queue {
  symbol: "q_open"
  signature: "int q_open(void)"
  effects: result -> OPEN
  success: result == 0
}

func q_close(q: Queue) -> int {
  symbol: "q_close"
  signature: "int q_close(int)"
  requires: q is OPEN
  effects: q -> CLOSED
  success: result == 0
}

func q_push_closed(q: Queue) -> int {
  symbol: "q_push1"
  signature: "int q_push1(int)"
  requires: q is CLOSED
  success: result < 0
}
