#ifndef DEFINES_H
#define DEFINES_H

#define GL_SILENCE_DEPRECATION

#define LOAD 25000.  // Приложенная нагрузка

#define FLT_MAX 100.
#define KOEF_X 25.
#define KOEF_Y 14.

extern const double h;
extern const double e;
extern const double puas;

extern int nelem;        // кол-во треугольных элементов
extern int nys;          // число узлов К.Э модели
extern double **car;     // массив координат узлов элемента
extern int **jt03;       // массив номеров узлов элемента
extern double *u;        // массив перемещений узлов
extern double **stress;  // массив напряжений

#endif