#directories and files
BUILD_TARGET := ./build

SRC_DIR := ./src
CODEGEN_IMPL_DIR := ./src/codegen-implementations
OBJ_DIR := ./obj

CODEGEN_IMPLENTATION ?= x86_64_linux_intel_asm

SRC := $(filter-out $(CODEGEN_IMPL_DIR)/*.c,$(wildcard $(SRC_DIR)/*.c)) #get src without codegen implentations
SRC := $(SRC) $(CODEGEN_IMPL_DIR)/$(CODEGEN_IMPLENTATION).c
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

OBJ_DIRS := $(sort $(dir $(OBJ)))

#flags
CC := gcc
CFLAGS := -g -O0 -Wall -Wextra
DFLAGS := -MMD -MP

#targets
all: $(BUILD_TARGET)

$(BUILD_TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIRS)
	$(CC) $(CFLAGS) $(DFLAGS) -c $< -o $@

$(OBJ_DIRS):
	mkdir -p $(OBJ_DIRS)

.PHONY: rebuild
rebuild: clean all

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BUILD_TARGET)

print:
	@echo $(SRC)
	@echo $(OBJ)
	@echo $(OBJ_DIRS)

#include dependencies
-include $(OBJ:.o=.d)