#ifndef DRAW_H
#define DRAW_H

#define GL_SILENCE_DEPRECATION

#include "../../dependencies/glew/2.2.01/include/GL/glew.h"
#include "../../dependencies/glfw/3.4/include/GLFW/glfw3.h"
#include "../../dependencies/glut/include/glut.h"

#define GLUT_WINDOW_POSITION_X 100  // Позиция окна (по оси x)
#define GLUT_WINDOW_POSITION_Y 100  // Позиция окна (по оси y)

#define WINDTH_WINDOW 1200      // Высота окна
#define HEIGHT_WINDOW 800       // Ширина окна
#define STRESS_SCALE 1000.0     // Масштаб для визуализации напряжений
#define DEFORMATION_SCALE 10.0  // Масштаб для визуализации деформаций

void drawMashForSolve(int argc, char **argv);
void drawModel(void);
void display(void);
void init(void);
void updateAnimation(int __attribute__((unused)) value);
void keyboard(unsigned char key, int __attribute__((unused)) x, int __attribute__((unused)) y);
void renderText(float x, float y, const char *text);

#endif