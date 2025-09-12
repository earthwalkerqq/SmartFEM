CC := clang
ARFLAGS := rcs

BUILD_FOLDER := ../build
OS := $(shell uname -s)

ifeq ($(OS), Darwin)
	LIBOMP_PATH := $(shell brew --prefix libomp)
	GLFW_PATH := $(shell brew --prefix glfw)
	GLEW_PATH := $(shell brew --prefix glew)
	CHECK_PATH := $(shell brew --prefix check)
	OPENMP_FLAG := -Xpreprocessor -fopenmp -Wno-deprecated-declarations
	MAC_FLAGS := -lm
else
	LIBOMP_PATH := /usr/lib/libomp
	GLFW_PATH := /usr/lib/libglfw
	GLEW_PATH := /usr/lib/libglew
	CHECK_PATH := /usr/lib/check
	OPENMP_FLAG := -fopenmp -Wno-deprecated-declarations
	LINUX_FLAGS := -lm -lrt -lpthread -lsubunit 
endif

CFLAGS := -Wall -Werror -Wextra -Iinc \
	-I$(LIBOMP_PATH)/include \
	-I$(CHECK_PATH)/include

GCOV_FLAGS := -fprofile-arcs -ftest-coverage

SRC_FILES := $(wildcard src/*.c)
OBJ_FILES := $(patsubst src/%.c,$(BUILD_FOLDER)/%.o,$(SRC_FILES))

TEST_FILES := $(wildcard tests/test_*.c)

FEM_LIB := $(BUILD_FOLDER)/libfem.a

OMP_FLAG := -L$(LIBOMP_PATH)/lib -lomp
GLFLAGS := -L$(GLFW_PATH)/lib -lglfw -framework OpenGL -framework GLUT -L$(GLEW_PATH)/lib -lglew
TEST_FLAG := $(OMP_FLAG) -L$(CHECK_PATH)/lib -lcheck $(OPENMP_FLAG)
FEM_FLAG := $(OMP_FLAG) $(OPENMP_FLAG) $(GLFLAGS)

.PHONY: all fem test clean rebuild check_deps style_check gcov_report

all: fem

# Проверка и установка зависимостей
check_deps:
ifeq ($(OS), Darwin)
	@echo "Checking dependencies for MacOS..."
	@brew update
	@if ! brew list libomp >/dev/null 2>&1; then brew install libomp; fi
	@if ! brew list glfw >/dev/null 2>&1; then brew install glfw; fi
	@if ! brew list glew >/dev/null 2>&1; then brew install glew; fi
	@if ! brew list check >/dev/null 2>&1; then brew install check; fi
else
	@echo "Checking dependencies for Linux..."
	@if ! dpkg -l | grep -q libomp-dev; then sudo apt update && sudo apt install -y libomp-dev; fi
	@if ! dpkg -l | grep -q libglfw3-dev; then sudo apt update && sudo apt install -y libglfw3-dev; fi
	@if ! dpkg -l | grep -q libglew-dev; then sudo apt update && sudo apt install -y libglew-dev; fi
	@if ! dpkg -l | grep -q check; then sudo apt update && sudo apt install -y check; fi
endif

$(BUILD_FOLDER):
	mkdir -p $(BUILD_FOLDER)

$(BUILD_FOLDER)/%.o: src/%.c | $(BUILD_FOLDER)
	$(CC) $(CFLAGS) -c $< -o $@

$(FEM_LIB): $(OBJ_FILES)
	ar $(ARFLAGS) $@ $^

fem: check_deps $(FEM_LIB)
ifeq ($(OS),Darwin)
	$(CC) $(CFLAGS) main.c $(FEM_LIB) -o $(BUILD_FOLDER)/fem $(MAC_FLAGS) $(FEM_FLAG)
else
	$(CC) $(CFLAGS) main.c $(FEM_LIB) -o $(BUILD_FOLDER)/fem $(LINUX_FLAGS) $(FEM_FLAG)
endif
	./$(BUILD_FOLDER)/fem

# Тесты
test: $(FEM_LIB)
ifeq ($(OS),Darwin)
	$(CC) $(CFLAGS) $(GCOV_FLAGS) $(TEST_FILES) $(FEM_LIB) -o test $(MAC_FLAGS) $(TEST_FLAG)
else
	$(CC) $(CFLAGS) $(GCOV_FLAGS) $(TEST_FILES) $(FEM_LIB) -o test $(LINUX_FLAGS) $(TEST_FLAG)
endif
	./test

# Отчёт gcov/lcov
gcov_report: clean
ifeq ($(OS), Darwin)
	$(CC) $(CFLAGS) $(GCOV_FLAGS) $(TEST_FILES) -o test $(MAC_FLAGS)
else
	$(CC) $(CFLAGS) $(GCOV_FLAGS) $(TEST_FILES) -o test $(LINUX_FLAGS)
endif
	./test
	rm -f test_*
	lcov -t "fem-01_test" -o postreport.info -c -d .
	genhtml -o report postreport.info
ifeq ($(OS), Darwin)
	open report/index.html
else
	xdg-open report/index.html
endif

style_check:
	cp ../materials/linters/.clang-format ./.clang-format
	clang-format -i *.c src/*.c inc/*.h
	clang-format -i src/tests/*.c src/tests/*.h
	rm -rf ./.clang-format

clean_obj:
	rm -rf obj/*.o *.o

clean_lib:
	rm -rf *.a

clean_fem:
	rm -rf fem

clean_test:
	rm -rf *.gcda
	rm -rf *.gcno
	rm -rf *.info
	rm -rf test
	rm -rf report
	rm -rf valgrind.txt

clean: clean_lib clean_obj clean_test clean_fem
	rm -rf unot_test
	rm -rf *.txt

rebuild: clean all
