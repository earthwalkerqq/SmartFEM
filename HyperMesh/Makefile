CC := clang
BUILD_FOLDER := ../build
OS := $(shell uname -s)

ifeq ($(OS), Darwin)
	GMSH_PATH := $(shell brew --prefix gmsh 2>/dev/null || echo /usr/local)
	GMSH_INC := $(GMSH_PATH)/include
	GMSH_LIB := $(GMSH_PATH)/lib
else
	GMSH_INC := /usr/local/include
	GMSH_LIB := /usr/local/lib
endif

CFLAGS := -Wall -Werror -Wextra -Iinc -I$(GMSH_INC) -DDIMENSION=3
ifeq ($(OS), Darwin)
	LDFLAGS := -L$(GMSH_LIB) -Wl,-rpath,$(GMSH_LIB) -lgmsh -lm
else
	LDFLAGS := -L$(GMSH_LIB) -Wl,-rpath,$(GMSH_LIB) -lgmsh -lm
endif

SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(patsubst src/%.c,$(BUILD_FOLDER)/hypermesh_%.o,$(SRC_FILES))

.PHONY: all clean rebuild check_gmsh

all: check_gmsh $(BUILD_FOLDER) $(BUILD_FOLDER)/hypermesh

check_gmsh:
	@if [ ! -f $(GMSH_LIB)/libgmsh.dylib ] && [ ! -f $(GMSH_LIB)/libgmsh.so ]; then \
		echo "Warning: Gmsh library not found at $(GMSH_LIB)"; \
		echo "Install Gmsh: brew install gmsh (macOS) or apt-get install libgmsh-dev (Linux)"; \
	fi

$(BUILD_FOLDER):
	mkdir -p $(BUILD_FOLDER)

$(BUILD_FOLDER)/hypermesh_%.o: src/%.c | $(BUILD_FOLDER)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_FOLDER)/hypermesh: main.c $(OBJ_FILES) | $(BUILD_FOLDER)
	$(CC) $(CFLAGS) main.c $(OBJ_FILES) -o $@ $(LDFLAGS)
	@echo "Built $@"

clean:
	rm -rf $(BUILD_FOLDER)/hypermesh_*.o $(BUILD_FOLDER)/hypermesh $(BUILD_FOLDER)/node.txt $(BUILD_FOLDER)/HyperMesh.msh

rebuild: clean all

