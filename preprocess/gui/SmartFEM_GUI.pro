QT += core widgets opengl openglwidgets

CONFIG += c++17

TARGET = SmartFEM_GUI
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    nodeselectionwindow.cpp \
    meshviewer.cpp

HEADERS += \
    mainwindow.h \
    nodeselectionwindow.h \
    meshviewer.h

# Используем относительные пути (от директории сборки)
DESTDIR = ../../build
OBJECTS_DIR = ../../build/gui_obj
MOC_DIR = ../../build/gui_moc
RCC_DIR = ../../build/gui_rcc

