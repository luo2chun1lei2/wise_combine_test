ANTLR_JAR := third_party/antlr-tool/antlr-4.13.2-complete.jar
ANTLR_RUNTIME := third_party/antlr4-runtime
GRAMMAR_DIR := src/grammar
GEN_DIR := build/gen
BUILD_DIR := build
BIN := $(BUILD_DIR)/wise_combine_test
STAMP := $(GEN_DIR)/.stamp

GRAMMARS := $(GRAMMAR_DIR)/FunctionDsl.g4 $(GRAMMAR_DIR)/StateMachineDsl.g4
RUNTIME_CPP := $(shell find $(ANTLR_RUNTIME) -name '*.cpp')
SRC_CPP := $(shell find src -name '*.cpp')

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I$(ANTLR_RUNTIME) -I$(GEN_DIR)/src/grammar

.PHONY: all test clean

all: $(BIN)

$(STAMP): $(GRAMMARS)
	mkdir -p $(GEN_DIR)
	java -jar $(ANTLR_JAR) -Dlanguage=Cpp -visitor -o $(GEN_DIR) $(GRAMMARS)
	touch $(STAMP)

$(BIN): $(STAMP) $(RUNTIME_CPP) $(SRC_CPP)
	$(CXX) $(CXXFLAGS) -o $@ $(RUNTIME_CPP) $$(find $(GEN_DIR) -name '*.cpp') $(SRC_CPP)

test: $(BIN)
	bash test/run.sh

clean:
	rm -rf $(BUILD_DIR)
