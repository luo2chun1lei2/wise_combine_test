# C++ 类方法映射示例
types {
  string -> "const char*"
  int -> "int"
}

values {
  datas: ["hello"]
}

class Store {
  cpp: "Store"
  header: "Store.h"
}

func put(data: string from datas) -> int {
  receiver: Store
  symbol: "put"
  signature: "int put(const char*)"
  requires:
  effects:
  success: result == 0
}

func get_count() -> int {
  receiver: Store
  symbol: "get_count"
  signature: "int get_count()"
  requires:
  effects:
  success: result >= 0
}
