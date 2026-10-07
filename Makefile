CC ?= gcc
SRC_DIR := src
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/cfg_bench

CFLAGS ?= -O2 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Wno-sign-compare

PREFIX ?= /usr/local
INCLUDES := -I$(PREFIX)/include -I$(PREFIX)/include/suitesparse -I$(SRC_DIR) -I$(SRC_DIR)/adapters
LDFLAGS := -L$(PREFIX)/lib64 -Wl,-rpath,$(PREFIX)/lib64
LDLIBS := -lgraphblas -llagraph -llagraphx
VALGRIND ?= valgrind
VALGRIND_OPTS ?= --leak-check=full --show-leak-kinds=definite,indirect --errors-for-leak-kinds=definite,indirect --error-exitcode=1

SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/adapters/*.c)
LIB_SRCS := $(filter-out $(SRC_DIR)/test.c,$(SRCS))

CFLAGS  += -pthread
LDFLAGS += -pthread

.PHONY: all clean bench debug test-explode-indices test-explode-indices-leaks

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) $(LDLIBS) -o $@

bench: $(TARGET)
	./$(TARGET) -c configs/configs_my.csv -r 1 --hot

CI: $(TARGET)
	./$(TARGET) -efblt -c configs/configs_my.csv

$(BUILD_DIR)/test_explode_indices: tests/test_explode_indices.c $(LIB_SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) $(LDLIBS) -o $(BUILD_DIR)/test_explode_indices

test-explode-indices: $(BUILD_DIR)/test_explode_indices
	./$(BUILD_DIR)/test_explode_indices

test-explode-indices-leaks: $(BUILD_DIR)/test_explode_indices
	$(VALGRIND) $(VALGRIND_OPTS) ./$(BUILD_DIR)/test_explode_indices

debug: clean
	$(MAKE) BUILD_DIR=build/debug \
		CFLAGS="-g -O0 -Wall -Wextra -Wpedantic -Wno-sign-compare"

# Code formatting with clang-format
FORMAT_SOURCES = src/*.c src/*.h src/adapters/*.c src/adapters/*.h tests/*.c

format:
	clang-format -i $(FORMAT_SOURCES)

clean:
	rm -rf build

REPEAT ?= 1
CONFIGS := configs_test

art_bench:
	@for c in $(CONFIGS); do \
		./build/cfg_bench -c configs/$$c.csv -a CFL -r $(REPEAT) --hot ; \
		./build/cfg_bench -c configs/$$c.csv -a CFL_Kron -r $(REPEAT) --hot ; \
	done
	

