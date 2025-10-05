#directories and files
BUILD_TARGET := ./build

SRC_DIR := ./src
OBJ_DIR := ./obj

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

#flags
CC := gcc
CFLAGS := -g -O0 -Wall -Wextra
DFLAGS := -MMD -MP

#targets
all: $(BUILD_TARGET)

$(BUILD_TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(DFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

.PHONY: rebuild
rebuild: clean all

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BUILD_TARGET)

#include dependencies
-include $(OBJ:.o=.d)