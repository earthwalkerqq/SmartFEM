#ifndef MESH_GENERATE_H
#define MESH_GENERATE_H

#ifdef SQUARE_GRID
#define MESH_ORDER 2
#else
#define MESH_ORDER 1
#endif

#ifndef DIMENSION
#define DIMENSION 2 // по умолчанию рассматривается 2D задача
#endif

typedef enum { FALSE = 0, TRUE } bool_t;

void initHyperMesh(char *inputFile, char *outputMeshFile, double clmin, double clmax, int *error);
bool_t hyperMesh(char *cadFile, double clmin, double clmax);

// Генерация сетки с заданным количеством элементов (итеративный подход)
// targetElements - желаемое количество элементов
// finalClmin, finalClmax - выходные параметры (фактические значения, использованные для генерации)
// actualElements - выходное значение (реальное количество сгенерированных элементов)
bool_t hyperMeshWithTargetElements(char *cadFile, int targetElements, 
                                   double *finalClmin, double *finalClmax, int *actualElements);

#endif