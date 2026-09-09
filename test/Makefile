CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
LDFLAGS ?= -ldl

OUT_DIR = out
SOURCES = test_wise.cpp ../src/wise.cpp
HEADERS = ../src/wise.hpp

TARGET = $(OUT_DIR)/test_wise
ASAN_TARGET = $(OUT_DIR)/test_wise_asan
COV_TARGET = $(OUT_DIR)/test_wise_cov
LIBTEST = $(OUT_DIR)/libtest.so

OBJS = $(OUT_DIR)/test_wise.o $(OUT_DIR)/wise.o
ASAN_OBJS = $(OUT_DIR)/test_wise_asan.o $(OUT_DIR)/wise_asan.o
COV_OBJS = $(OUT_DIR)/test_wise_cov.o $(OUT_DIR)/wise_cov.o

.PHONY: all clean asan coverage

all: $(TARGET) $(LIBTEST)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OUT_DIR)/test_wise.o: test_wise.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT_DIR)/wise.o: ../src/wise.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIBTEST): fixtures/libtest.cpp | $(OUT_DIR)
	$(CXX) -std=c++17 -O2 -fPIC -shared fixtures/libtest.cpp -o $(LIBTEST)

asan: $(ASAN_TARGET) $(LIBTEST)

$(ASAN_TARGET): $(ASAN_OBJS)
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer $(ASAN_OBJS) -o $(ASAN_TARGET) $(LDFLAGS)

$(OUT_DIR)/test_wise_asan.o: test_wise.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer -c $< -o $@

$(OUT_DIR)/wise_asan.o: ../src/wise.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address -fno-omit-frame-pointer -c $< -o $@

coverage: $(COV_TARGET) $(LIBTEST)

$(COV_TARGET): $(COV_OBJS)
	$(CXX) -std=c++17 -O0 -g --coverage $(COV_OBJS) -o $(COV_TARGET) $(LDFLAGS)

$(OUT_DIR)/test_wise_cov.o: test_wise.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) -std=c++17 -O0 -g --coverage -c $< -o $@

$(OUT_DIR)/wise_cov.o: ../src/wise.cpp $(HEADERS) | $(OUT_DIR)
	$(CXX) -std=c++17 -O0 -g --coverage -c $< -o $@

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

clean:
	rm -rf $(OUT_DIR)
