static int g_open = 0;

int open_h(const char *name) {
  (void)name;
  g_open = 1;
  return 1;
}

int close_h(int handle) {
  (void)handle;
  g_open = 0;
  return 0;
}

const char *handle_state(int handle) {
  (void)handle;
  return g_open ? "OPEN" : "CLOSED";
}
