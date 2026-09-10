# libc 文件函数组示例：fopen / fwrite / fread / fclose
# 用途：实践和扩展函数调用序列 DSL。
#
# 此示例暴露的待扩展点：
# 1. 需要支持更多值类型映射，例如 buffer -> void*、usize -> size_t。
# 2. success 表达式需要支持指针判空，例如 result != NULL。
# 3. fread/fwrite 的缓冲区参数在 DSL 中暂按普通输入值参数处理。

types {
  string -> const char*
  int -> int
  usize -> size_t
  buffer -> void*
}

values {
  paths: ["a.txt", "b.txt", "c.txt"]
  modes: ["r", "w", "a"]
  sizes: 1..64
}

resource File {
  ctype: FILE*
  states: CLOSED, OPEN
  initial: CLOSED
}

func fopen(path: string from paths, mode: string from modes) -> File {
  symbol: "fopen"
  signature: "FILE* fopen(const char *, const char *)"
  requires:
  effects: result -> OPEN
  success: result != NULL
}

func fwrite(ptr: buffer, size: usize from sizes, nmemb: usize from sizes, f: File) -> usize {
  symbol: "fwrite"
  signature: "size_t fwrite(const void *, size_t, size_t, FILE *)"
  requires: f is OPEN
  effects: f -> OPEN
  success: result == nmemb
}

func fread(ptr: buffer, size: usize from sizes, nmemb: usize from sizes, f: File) -> usize {
  symbol: "fread"
  signature: "size_t fread(void *, size_t, size_t, FILE *)"
  requires: f is OPEN
  effects: f -> OPEN
  success: result == nmemb
}

func fclose(f: File) -> int {
  symbol: "fclose"
  signature: "int fclose(FILE *)"
  requires: f is OPEN
  effects: f -> CLOSED
  success: result == 0
}
