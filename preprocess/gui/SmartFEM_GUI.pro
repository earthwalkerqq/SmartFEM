QT += core widgets opengl openglwidgets

CONFIG += c++17

TARGET = SmartFEM_GUI
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    nodeselectionwindow.cpp \
    meshviewer.cpp \
    meshviewerwindow.cpp \
    meshgenerator.cpp

HEADERS += \
    mainwindow.h \
    nodeselectionwindow.h \
    meshviewer.h \
    meshviewerwindow.h \
    meshgenerator.h

# Используем относительные пути (от директории сборки)
DESTDIR = ../../build
OBJECTS_DIR = ../../build/gui_obj
MOC_DIR = ../../build/gui_moc
RCC_DIR = ../../build/gui_rcc

# Gmsh library paths
macx {
    GMSH_PATH = $$system(brew --prefix gmsh 2>/dev/null || echo /usr/local)
    isEmpty(GMSH_PATH) {
        GMSH_PATH = /usr/local
    }
    GMSH_INC = $${GMSH_PATH}/include
    GMSH_LIB = $${GMSH_PATH}/lib
    
    INCLUDEPATH += $${GMSH_INC}
    LIBS += -L$${GMSH_LIB} -lgmsh
    QMAKE_RPATHDIR += $${GMSH_LIB}
    
    # GLM (header-only library)
    GLM_PATH = $$system(brew --prefix glm 2>/dev/null || echo /usr/local)
    isEmpty(GLM_PATH) {
        GLM_PATH = /usr/local
    }
    INCLUDEPATH += $${GLM_PATH}/include
}

unix:!macx {
    GMSH_INC = /usr/local/include
    GMSH_LIB = /usr/local/lib
    
    INCLUDEPATH += $${GMSH_INC}
    LIBS += -L$${GMSH_LIB} -lgmsh
    QMAKE_RPATHDIR += $${GMSH_LIB}
}

