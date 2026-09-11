#include "Store.h"

int Store::put(const char *data) {
  (void)data;
  ++count_;
  return 0;
}

int Store::get_count() const {
  return count_;
}
