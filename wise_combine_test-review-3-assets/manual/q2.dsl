types { int -> "int" }
resource Queue { ctype: "int" states: CLOSED, OPEN, ONE, TWO initial: CLOSED }
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
func q_push2(q: Queue) -> int {
 symbol: "q_push2"
 signature: "int q_push2(int)"
 requires: q is ONE
 effects: q -> TWO
 success: result == 0
}
func q_pop_two(q: Queue) -> int {
 symbol: "q_pop"
 signature: "int q_pop(int)"
 requires: q is TWO
 effects: q -> ONE
 success: result == 11
}
func q_pop_one(q: Queue) -> int {
 symbol: "q_pop"
 signature: "int q_pop(int)"
 requires: q is ONE
 effects: q -> CLOSED
 success: result == 22
}
