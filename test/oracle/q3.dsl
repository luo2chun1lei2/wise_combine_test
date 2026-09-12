types { int -> "int" }
resource Queue { ctype: "int" states: CLOSED, OPEN, NONEMPTY initial: CLOSED }

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
  effects: q -> NONEMPTY
  success: result == 0
}

func q_close(q: Queue) -> int {
  symbol: "q_close"
  signature: "int q_close(int)"
  requires: q is NONEMPTY
  effects: q -> CLOSED
  success: result == 0
}

func q_reopen(q: Queue) -> int {
  symbol: "q_reopen"
  signature: "int q_reopen(int)"
  requires: q is CLOSED
  effects: q -> OPEN
  success: result == 0
}

func q_size_nonempty(q: Queue) -> int {
  symbol: "q_size"
  signature: "int q_size(int)"
  requires: q is OPEN
  success: result == 0
}
