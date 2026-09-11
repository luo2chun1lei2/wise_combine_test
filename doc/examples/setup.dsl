# 预创建多个句柄的示例
types {
  string -> "const char*"
  int -> "int"
}

values {
  names: ["a"]
}

resource Handle {
  ctype: "int"
  states: CLOSED, OPEN
  initial: CLOSED
}

setup {
  handles: open_h("a") x 2
}

func open_h(name: string from names) -> Handle {
  symbol: "open_h"
  signature: "int open_h(const char*)"
  requires:
  effects: result -> OPEN
  success: result >= 0
}

func close_h(h: Handle) -> int {
  symbol: "close_h"
  signature: "int close_h(int)"
  requires: h is OPEN
  effects: h -> CLOSED
  success: result == 0
}
