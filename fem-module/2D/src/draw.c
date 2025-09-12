#include "draw.h"

#include <math.h>
#include <stdio.h>
#include <stdarg.h>

#include "defines.h"

// Глобальные переменные для управления визуализацией
static int showDeformed = 1;           // Показывать деформированную модель
static int showStress = 1;             // Показывать напряжения
static int showValues = 0;             // Показывать числовые значения
static double zoom = 1.0;              // Масштаб отображения
static float animationProgress = 0.f;  // Шаг анимации
static int isAnimating = 0;            // Флаг анимации
static int animationDirection = 1;     // Направление анимации

void drawMashForSolve(int argc, char **argv) {
    // Инициализация GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(WINDTH_WINDOW, HEIGHT_WINDOW);
    glutInitWindowPosition(GLUT_WINDOW_POSITION_X, GLUT_WINDOW_POSITION_Y);
    glutCreateWindow("FEM Visualization");
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    init();
    glutMainLoop();
}

// функция отрисовки конечных элементов модели
void drawModel(void) {
    for (int i = 0; i < nelem; i++) {
        // Вычисление среднего напряжения для элемента
        int ielem = jt03[0][i];
        double avgStress = 0.0;
        for (int j = 0; j < 3; j++) {
            avgStress += stress[j][ielem - 1];
        }
        avgStress /= 3.0;
        // Нормализация напряжения для цветовой схемы
        double normalizedStress = fabs(avgStress) / STRESS_SCALE;
        normalizedStress = (normalizedStress > 1.0) ? 1.0 : normalizedStress;
        // Установка цвета в зависимости от режима отображения
        if (showStress) {
            glColor3f(normalizedStress, 0.0, 1.0 - normalizedStress);
        } else {
            glColor3f(0.0, 0.5, 0.0);
        }
        // Отрисовка треугольника
        glBegin(GL_TRIANGLES);
        for (int j = 0; j < 3; j++) {
            double x = car[0][jt03[j][i] - 1];
            double y = car[1][jt03[j][i] - 1];
            if (showDeformed) {
                if (x != 0. && y != 0) {  // опираясь на схему нагружения образца
                    x += u[jt03[j][i] * 2 - 2] * DEFORMATION_SCALE * animationProgress;
                    y += u[jt03[j][i] * 2 - 1] * DEFORMATION_SCALE * animationProgress;
                }
            }
            glVertex2f(KOEF_X + x * zoom / 2., KOEF_Y + y * zoom / 2.);
        }
        glEnd();
        // Отрисовка границ элементов
        glColor3f(0.7, 0.7, 0.7);
        int index = 0;
        for (int j = 0; j < 3; j++) {
            glBegin(GL_LINES);
            double x1 = car[0][jt03[index][i] - 1];
            double y1 = car[1][jt03[index][i] - 1];
            double x2 = car[0][jt03[(index + 1) % 3][i] - 1];
            double y2 = car[1][jt03[(index + 1) % 3][i] - 1];
            if (showDeformed) {
                if (x1 != 0. && y1 != 0) {
                    x1 += u[jt03[index][i] * 2 - 2] * DEFORMATION_SCALE * animationProgress;
                    y1 += u[jt03[index][i] * 2 - 1] * DEFORMATION_SCALE * animationProgress;
                }
                if (x2 != 0. && y2 != 0) {
                    x2 += u[jt03[(index + 1) % 3][i] * 2 - 2] * DEFORMATION_SCALE * animationProgress;
                    y2 += u[jt03[(index + 1) % 3][i] * 2 - 1] * DEFORMATION_SCALE * animationProgress;
                }
            }
            glVertex2f(KOEF_X + x1 * zoom / 2., KOEF_Y + y1 * zoom / 2.);
            glVertex2f(KOEF_X + x2 * zoom / 2., KOEF_Y + y2 * zoom / 2.);
            glEnd();
            index = (index + 1) % 3;
        }
        // Отображение числовых значений напряжений
        if (showValues) {
            char text[32];
            sprintf(text, "%.1f", avgStress);
            double centerX = 0.0, centerY = 0.0;
            for (int j = 0; j < 3; j++) {
                centerX += car[0][jt03[j][i] - 1];
                centerY += car[1][jt03[j][i] - 1];
            }
            centerX = KOEF_X + (centerX / 3.0) * zoom / 2.;
            centerY = KOEF_Y + (centerY / 3.0) * zoom / 2.;
            glColor3f(0.0, 0.0, 0.0);
            renderText(centerX, centerY, text);
        }
    }
}

void display(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    drawModel();
    glFlush();
}

void init(void) {
    double minX = FLT_MAX, maxX = -FLT_MAX;
    double minY = FLT_MAX, maxY = -FLT_MAX;
    for (int i = 0; i < nys; i++) {
        if (car[0][i] < minX) minX = car[0][i];
        if (car[0][i] > maxX) maxX = car[0][i];
        if (car[1][i] < minY) minY = car[1][i];
        if (car[1][i] > maxY) maxY = car[1][i];
    }
    gluOrtho2D(minX, maxX, minY, maxY);
    glClearColor(1., 1., 1., 1.);  // Белый фон
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
}
void updateAnimation(int __attribute__((unused)) value) {
    if (isAnimating) {
        animationProgress += 0.01f * animationDirection;
        if (animationProgress >= 1.0f) {
            animationProgress = 1.0f;
            animationDirection = -1;
        } else if (animationProgress <= 0.0f) {
            animationProgress = 0.0f;
            animationDirection = 1;
        }
        glutPostRedisplay();
        glutTimerFunc(16, updateAnimation, 0);
    }
}

void keyboard(unsigned char key, int __attribute__((unused)) x, int __attribute__((unused)) y) {
    switch (key) {
        case 'd':  // Переключение деформированной/недеформированной модели
            showDeformed = !showDeformed;
            break;
        case 's':  // Переключение отображения напряжений
            showStress = !showStress;
            break;
        case 'v':  // Переключение отображения числовых значений
            showValues = !showValues;
            break;
        case '=':  // Увеличение масштаба (чтобы не зажимать shift)
            zoom *= 1.1;
            break;
        case '-':  // Уменьшение масштаба
            zoom /= 1.1;
            break;
        case 'a':  // Включение/выключение анимации
            isAnimating = !isAnimating;
            if (isAnimating) {
                animationProgress = 0.0f;
                animationDirection = 1;
                glutTimerFunc(16, updateAnimation, 0);
            }
            break;
    }
    glutPostRedisplay();
}

// Функция для отображения текста
void renderText(float x, float y, const char *text) {
    glRasterPos2f(x, y);
    for (const char *c = text; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, *c);
    }
}