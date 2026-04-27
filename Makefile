CC ?= gcc
SRC_DIR := src

BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/cfg_bench

CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare
INCLUDES := -I/usr/local/include/suitesparse -I$(SRC_DIR) -I$(SRC_DIR)/adapters
LDLIBS := -lgraphblas -llagraph -llagraphx

SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/adapters/*.c)

.PHONY: all clean bench debug

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDLIBS) -o $@

bench: $(TARGET)
	./$(TARGET) -c configs/configs_my.csv -r 10 --hot

CI: $(TARGET)
	./$(TARGET) -efblt -c configs/configs_my.csv

debug: clean
	$(MAKE) BUILD_DIR=build/debug \
		CFLAGS="-g -O0 -Wall -Wextra -Wpedantic -Wno-sign-compare"

clean:
	rm -rf build