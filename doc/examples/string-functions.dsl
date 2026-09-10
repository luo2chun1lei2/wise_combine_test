# libc 字符串处理函数组示例（string.h）：strlen / strcpy / strcat / strcmp
# 用途：实践和扩展函数调用序列 DSL。
#
# 此示例暴露的待扩展点：
# 1. strlen、strcmp 等函数没有“失败”概念，需要无失败或恒真 success 语义。
# 2. strcpy、strcat 的 dest 是输出缓冲区，不是普通输入值参数。
# 3. 这些函数没有句柄生命周期，属于“无资源函数”，需要确认 DSL 是否支持。

types {
  string -> "const char*"
  cstring -> "char*"
  usize -> "size_t"
  int -> "int"
}

values {
  strings: ["hello", "world", ""]
}

func strlen(s: string from strings) -> usize {
  symbol: "strlen"
  signature: "size_t strlen(const char *)"
  requires:
  effects:
  success: result >= 0
}

func strcpy(dest: cstring, src: string from strings) -> cstring {
  symbol: "strcpy"
  signature: "char* strcpy(char *, const char *)"
  requires:
  effects:
  success: result == dest
}

func strcat(dest: cstring, src: string from strings) -> cstring {
  symbol: "strcat"
  signature: "char* strcat(char *, const char *)"
  requires:
  effects:
  success: result == dest
}

func strcmp(s1: string from strings, s2: string from strings) -> int {
  symbol: "strcmp"
  signature: "int strcmp(const char *, const char *)"
  requires:
  effects:
  success: true
}
