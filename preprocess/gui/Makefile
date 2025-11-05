CC := clang++
BUILD_FOLDER := ../../build
OS := $(shell uname -s)

# Qt paths
ifeq ($(OS), Darwin)
	QT_PATH := $(shell brew --prefix qt@6 2>/dev/null || brew --prefix qt5 2>/dev/null || echo /usr/local/qt)
	QT_INC := $(QT_PATH)/include
	QT_LIB := $(QT_PATH)/lib
	MOC := $(QT_PATH)/bin/moc
else
	QT_PATH := /usr/lib/qt6
	QT_INC := /usr/include/qt6
	QT_LIB := /usr/lib/qt6
	MOC := moc-qt6
endif

# Проверка наличия Qt
QMAKE := $(shell which qmake 2>/dev/null || echo $(QT_PATH)/bin/qmake)
MOC := $(shell which moc 2>/dev/null || echo $(MOC))

CFLAGS := -Wall -Wextra -std=c++17 -fPIC
LDFLAGS := -L$(QT_LIB)

# Qt modules
QT_MODULES := Core Widgets

.PHONY: all clean rebuild check_qt install_qt

all: check_qt $(BUILD_FOLDER)/SmartFEM_GUI

check_qt:
	@if [ -z "$(shell which qmake 2>/dev/null)" ] && [ ! -f "$(QT_PATH)/bin/qmake" ]; then \
		echo "Qt not found. Trying to install..."; \
		echo "  macOS: brew install qt@6"; \
		echo "  Linux: sudo apt-get install qt6-base-dev"; \
		echo ""; \
		echo "Or use qmake directly:"; \
		echo "  qmake SmartFEM_GUI.pro"; \
		echo "  make"; \
		exit 1; \
	fi

install_qt:
ifeq ($(OS), Darwin)
	@echo "Installing Qt via Homebrew..."
	brew install qt@6
else
	@echo "Installing Qt..."
	sudo apt-get update && sudo apt-get install -y qt6-base-dev qmake6
endif

$(BUILD_FOLDER):
	mkdir -p $(BUILD_FOLDER)

# Используем qmake для сборки Qt проекта
$(BUILD_FOLDER)/SmartFEM_GUI: $(BUILD_FOLDER) SmartFEM_GUI.pro main.cpp mainwindow.cpp mainwindow.h
	@if [ -n "$(shell which qmake 2>/dev/null)" ]; then \
		cd $(BUILD_FOLDER) && qmake ../preprocess/gui/SmartFEM_GUI.pro && $(MAKE); \
	else \
		$(QMAKE) SmartFEM_GUI.pro -o $(BUILD_FOLDER)/Makefile.gui; \
		$(MAKE) -f $(BUILD_FOLDER)/Makefile.gui -C $(BUILD_FOLDER); \
	fi
	@echo "Built $(BUILD_FOLDER)/SmartFEM_GUI"

clean:
	rm -rf $(BUILD_FOLDER)/SmartFEM_GUI $(BUILD_FOLDER)/*.o $(BUILD_FOLDER)/gui_* $(BUILD_FOLDER)/Makefile.gui $(BUILD_FOLDER)/moc_*.cpp

rebuild: clean all

