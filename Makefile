# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -I./include
LDFLAGS =

# Optimization levels
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG -march=native
PERFORMANCE_FLAGS = -O3 -DNDEBUG -march=native -flto

# Default to debug mode
CXXFLAGS += $(DEBUG_FLAGS)

# Flex and Bison
FLEX = flex
BISON = bison
BISON_FLAGS = -d -v

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Source files
PARSER_Y = $(SRC_DIR)/parser.y
LEXER_FLEX = $(SRC_DIR)/lexer.flex
SCAN_CONST_CPP = $(SRC_DIR)/optimizations/scan_const.cpp
SCAN_UNUSED_CPP = $(SRC_DIR)/optimizations/scan_unused.cpp
MAIN_CPP = $(SRC_DIR)/main.cpp

# Generated files
PARSER_HPP = $(BUILD_DIR)/parser.hpp
PARSER_CPP = $(BUILD_DIR)/parser.cpp
LEXER_CPP = $(BUILD_DIR)/lexer.cpp

# Object files
MAIN_O = $(BUILD_DIR)/main.o
PARSER_O = $(BUILD_DIR)/parser.o
LEXER_O = $(BUILD_DIR)/lexer.o

# Target executable
TARGET = main

# All targets
all: $(TARGET)

# Link the executable
$(TARGET): $(MAIN_O) $(PARSER_O) $(LEXER_O)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Compiler built successfully: $@"

# Compile main.cpp
$(MAIN_O): $(MAIN_CPP) $(SCAN_CONST_CPP) $(SCAN_UNUSED_CPP) $(PARSER_HPP) $(INCLUDE_DIR)/ast.hpp $(INCLUDE_DIR)/context.hpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile generated parser
$(PARSER_O): $(PARSER_CPP) $(PARSER_HPP) $(INCLUDE_DIR)/ast.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile generated lexer
$(LEXER_O): $(LEXER_CPP) $(PARSER_HPP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Generate parser from .y file
$(PARSER_CPP) $(PARSER_HPP): $(PARSER_Y)
	@mkdir -p $(BUILD_DIR)
	$(BISON) $(BISON_FLAGS) $< -o $(PARSER_CPP) --defines=$(PARSER_HPP)

# Generate lexer from .flex file
$(LEXER_CPP): $(LEXER_FLEX) $(PARSER_HPP)
	@mkdir -p $(BUILD_DIR)
	$(FLEX) -o $@ $<

# Clean up build files
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Build targets for different optimization levels
debug: CXXFLAGS = -std=c++17 -Wall -Wextra -I./include $(DEBUG_FLAGS)
debug: $(TARGET)

release: CXXFLAGS = -std=c++17 -Wall -Wextra -I./include $(RELEASE_FLAGS)
release: $(TARGET)

performance: CXXFLAGS = -std=c++17 -Wall -Wextra -I./include $(PERFORMANCE_FLAGS)
performance: $(TARGET)

# Test targets
test: $(TARGET)
	@echo "Testing simple case..."
	@./$(TARGET) test_simple.tc > output_simple.s
	@echo "Testing comprehensive case..."
	@./$(TARGET) test_comprehensive.tc > output_comprehensive.s
	@echo "Testing optimization case..."
	@./$(TARGET) test_optimization.tc > output_optimization.s
	@echo "Generated assembly files: output_*.s"

# Benchmark target
benchmark: performance
	@echo "Running performance benchmark..."
	@time ./$(TARGET) test_comprehensive.tc > /dev/null
	@echo "Benchmark completed."

.PHONY: all clean debug release performance test benchmark