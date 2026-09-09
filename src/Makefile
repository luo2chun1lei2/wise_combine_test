CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
LDFLAGS ?= -ldl

OUT_DIR = out
SOURCES = main.cpp wise.cpp
HEADERS = wise.hpp

TARGET = $(OUT_DIR)/wise_combine_test
ASAN_TARGET = $(OUT_DIR)/wise_combine_test_asan
OBJS = $(OUT_DIR)/main.o $(OUT_DIR)/wise.o
ASAN_OBJS = $(OUT_DIR)/main_asan.o $(OUT_DIR)/wise_asan.o

.PHONY: all clean asan

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OUT_DIR)/%.o: %.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

asan: $(ASAN_TARGET)

$(ASAN_TARGET): $(ASAN_OBJS)
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer $(ASAN_OBJS) -o $(ASAN_TARGET) $(LDFLAGS)

$(OUT_DIR)/%_asan.o: %.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer -c $< -o $@

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

clean:
	rm -rf $(OUT_DIR)
