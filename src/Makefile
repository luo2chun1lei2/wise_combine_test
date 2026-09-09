CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
LDFLAGS ?= -ldl

SOURCES = main.cpp wise.cpp
HEADERS = wise.hpp
TARGET = wise_combine_test

.PHONY: all clean asan

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

asan:
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer $(SOURCES) -o $(TARGET)_asan $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET)_asan
