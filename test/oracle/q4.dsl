types { int -> "int" }
resource Queue { ctype: "int" states: CLOSED, OPEN, ONE initial: CLOSED }

func q_open() -> Queue {
  symbol: "q_open"
  signature: "int q_open(void)"
  effects: result -> OPEN
  success: result == 0
}

func q_push1(q: Queue) -> int {
  symbol: "q_push1"
  signature: "int q_push1(int)"
  requires: q is OPEN
  effects: q -> ONE
  success: result == 0
}

func q_peek(q: Queue) -> int {
  symbol: "q_peek"
  signature: "int q_peek(int)"
  requires: q is ONE
  effects: q -> ONE
  success: result == 11
}

func q_size_one(q: Queue) -> int {
  symbol: "q_size"
  signature: "int q_size(int)"
  requires: q is ONE
  success: result == 1
}
