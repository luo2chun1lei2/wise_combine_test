CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

SOURCES = test_wise.cpp ../src/wise.cpp
TARGET = test_wise

.PHONY: all clean asan coverage

all: $(TARGET) libtest.so

$(TARGET): $(SOURCES) ../src/wise.hpp
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) -ldl

libtest.so: fixtures/libtest.cpp
	$(CXX) -std=c++17 -O2 -fPIC -shared fixtures/libtest.cpp -o libtest.so

asan: libtest.so
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer $(SOURCES) -o $(TARGET)_asan -ldl

coverage: libtest.so
	$(CXX) -std=c++17 -O0 -g --coverage $(SOURCES) -o $(TARGET)_cov -ldl

clean:
	rm -f $(TARGET) $(TARGET)_asan $(TARGET)_cov libtest.so *.gcno *.gcda *.gcov
