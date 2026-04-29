CC ?= gcc
SRC_DIR := src

BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/cfg_bench

CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare
INCLUDES := -I/usr/local/include/suitesparse -I$(SRC_DIR) -I$(SRC_DIR)/adapters
LDLIBS := -lgraphblas -llagraph -llagraphx
VALGRIND ?= valgrind
VALGRIND_OPTS ?= --leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=1

SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/adapters/*.c)
LIB_SRCS := $(filter-out $(SRC_DIR)/test.c,$(SRCS))

.PHONY: all clean bench debug test-explode-indices test-explode-indices-leaks

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDLIBS) -o $@

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
	$(MAKE) BUILD_DIR=build/debug \
		CFLAGS="-g -O0 -Wall -Wextra -Wpedantic -Wno-sign-compare"

clean:
	rm -rf build
