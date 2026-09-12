types { int -> "int" }
values { pushes: ["11", "22"] }

resource Queue {
  ctype: "int"
  states: CLOSED, OPEN
  initial: CLOSED
  list items = []
}

func q_open() -> Queue {
  symbol: "q_open"
  signature: "int q_open(void)"
  effects: result -> OPEN
  success: result == 0
}

func q_push(v: int from pushes) -> int {
  symbol: "q_pushv"
  signature: "int q_pushv(int)"
  update: items << v
  success: result == 0
}

func q_pop(q: Queue) -> int {
  symbol: "q_pop"
  signature: "int q_pop(int)"
  requires: q is OPEN
  effects: q -> OPEN
  update: items >>
  success: result == front(items)
}

func q_size(q: Queue) -> int {
  symbol: "q_size"
  signature: "int q_size(int)"
  requires: q is OPEN
  success: result == len(items)
}
