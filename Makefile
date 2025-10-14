#directories and files
BUILD_TARGET := ./build

SRC_DIR := ./src
OBJ_DIR := ./obj

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

OBJ_DIRS := $(sort $(dir $(OBJ)))

#flags
CC := clang

CFLAGS := -g -O0 -Wall -Wextra -pedantic
DFLAGS := -MMD -MP

LLVM_CFLAGS := $(shell llvm-config --cflags)
LLVM_LFLAGS := $(shell llvm-config --ldflags) -lpthread -ldl -lz
LLVM_LIB := $(shell llvm-config --libs core bitwriter irreader native)

#targets
all: $(BUILD_TARGET)

$(BUILD_TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LLVM_CFLAGS) $(LLVM_LFLAGS) $^ $(LLVM_LIB) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIRS)
	$(CC) $(CFLAGS) $(LLVM_CFLAGS) $(DFLAGS) -c $< -o $@

$(OBJ_DIRS):
	mkdir -p $(OBJ_DIRS)

.PHONY: rebuild
rebuild: clean all

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BUILD_TARGET)

-include $(OBJ:.o=.d)