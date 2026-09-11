#pragma once

class Store {
 public:
  int put(const char *data);
  int get_count() const;

 private:
  int count_ = 0;
};
