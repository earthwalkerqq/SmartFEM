CC := clang

BUILD_FOLDER := ../build

OS := $(shell uname -s)

CFLAGS := -Wall -Werror -Wextra -Iinc -I../2D/inc -I../2D/utils
LDFLAGS := -lm ../build/libfem.a

SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(patsubst src/%.c,$(BUILD_FOLDER)/modal_%.o,$(SRC_FILES))

.PHONY: all clean rebuild

FEM_LIB := ../build/libfem.a

all: $(BUILD_FOLDER) $(FEM_LIB) $(BUILD_FOLDER)/modal

$(BUILD_FOLDER):
	mkdir -p $(BUILD_FOLDER)

$(FEM_LIB):
	$(MAKE) -C ../2D $(FEM_LIB)

$(BUILD_FOLDER)/modal_%.o: src/%.c | $(BUILD_FOLDER)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_FOLDER)/modal: main.c $(OBJ_FILES) | $(BUILD_FOLDER) $(FEM_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Built $@"

clean:
	rm -rf $(BUILD_FOLDER)/modal_*.o $(BUILD_FOLDER)/modal

rebuild: clean all

