ANTLR_JAR := third_party/antlr-tool/antlr-4.13.2-complete.jar
ANTLR_RUNTIME := third_party/antlr4-runtime
GRAMMAR_DIR := src/grammar
GEN_DIR := build/gen
OBJ_DIR := build/obj
BUILD_DIR := build
BIN := $(BUILD_DIR)/wise_combine_test
STAMP := $(GEN_DIR)/.stamp

GRAMMARS := $(GRAMMAR_DIR)/FunctionDsl.g4 $(GRAMMAR_DIR)/StateMachineDsl.g4
RUNTIME_CPP := $(shell find $(ANTLR_RUNTIME) -name '*.cpp')
SRC_CPP := $(shell find src -name '*.cpp')
RUNTIME_OBJ := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(RUNTIME_CPP))

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I$(ANTLR_RUNTIME) -I$(GEN_DIR)/src/grammar

.PHONY: all test install clean

all: $(BIN)

$(STAMP): $(GRAMMARS)
	mkdir -p $(GEN_DIR)
	java -jar $(ANTLR_JAR) -Dlanguage=Cpp -visitor -o $(GEN_DIR) $(GRAMMARS)
	touch $(STAMP)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BIN): $(STAMP) $(RUNTIME_OBJ) $(SRC_CPP)
	$(CXX) $(CXXFLAGS) -o $@ $(RUNTIME_OBJ) $$(find $(GEN_DIR) -name '*.cpp') $(SRC_CPP)

test: $(BIN)
	bash test/run.sh

PREFIX ?= /usr/local

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -rf $(BUILD_DIR)
