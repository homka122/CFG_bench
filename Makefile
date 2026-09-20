CC ?= gcc
SRC_DIR := src

BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/cfg_bench
PAIRS_EXTRACTOR_TARGET := $(BUILD_DIR)/pairs_extractor

CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare
INCLUDES := -I/usr/local/include/suitesparse -I$(SRC_DIR) -I$(SRC_DIR)/adapters
LDLIBS := -lgraphblas -llagraph -llagraphx
VALGRIND ?= valgrind
VALGRIND_OPTS ?= --leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=1

SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/adapters/*.c)
MAIN_SRCS := $(SRC_DIR)/test.c $(SRC_DIR)/pairs_extractor.c
LIB_SRCS := $(filter-out $(MAIN_SRCS),$(SRCS))

.PHONY: all clean bench debug pairs-extractor test-explode-indices test-explode-indices-leaks

all: $(TARGET) $(PAIRS_EXTRACTOR_TARGET)

$(TARGET): $(SRC_DIR)/test.c $(LIB_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDLIBS) -o $@

$(PAIRS_EXTRACTOR_TARGET): $(SRC_DIR)/pairs_extractor.c $(LIB_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDLIBS) -o $@

pairs-extractor: $(PAIRS_EXTRACTOR_TARGET)

bench: $(TARGET)
	./$(TARGET) -c configs/configs_my.csv -r 10 --hot

CI: $(TARGET)
	./$(TARGET) -efblt -c configs/configs_my.csv

$(BUILD_DIR)/test_explode_indices: tests/test_explode_indices.c $(LIB_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDLIBS) -o $(BUILD_DIR)/test_explode_indices

test-explode-indices: $(BUILD_DIR)/test_explode_indices
	./$(BUILD_DIR)/test_explode_indices

test-explode-indices-leaks: $(BUILD_DIR)/test_explode_indices
	$(VALGRIND) $(VALGRIND_OPTS) ./$(BUILD_DIR)/test_explode_indices

debug: clean
	$(MAKE) BUILD_DIR=build \
		CFLAGS="-g -O0 -Wall -Wextra -Wpedantic -Wno-sign-compare"

# Code formatting with clang-format
FORMAT_SOURCES = src/*.c src/*.h src/adapters/*.c src/adapters/*.h tests/*.c

format:
	clang-format -i $(FORMAT_SOURCES)

clean:
	rm -rf build/*
	touch build/.gitkeep
