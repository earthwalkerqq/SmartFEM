#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "LDLT.h"
#include "bc.h"
#include "defines.h"
#include "draw.h"
#include "fem.h"
#include "io.h"
#include "mtrx.h"

/*--------------------------------------------------------*/
// Необходимо проработать получение закрепленных и нагруженных узлов
// через сеты внутри файла с узлами.
/*--------------------------------------------------------*/

int nelem;
int nys;
double **car = NULL;
int **jt03 = NULL;
double *u = NULL;
double **stress = NULL;

// задаем материал
const double h = 1.;
const double e = 2.1e5;
const double puas = 0.3;

int main(int argc, char **argv) {
    const int ndofysla = 2;  // кол-во степеней свободы одного узла
    double *dataCar;
    int *data_jt03;

    const char *outputFile = "./build/result.txt";
    const char *inputFile = (argc == 2) ? argv[1] : "../data-sample/node.txt"; // !!!

    short fileErr = readFromFile(inputFile, &nys, &dataCar, &car, &nelem, &data_jt03, &jt03);
    if (fileErr == 1) {
        free_memory(4, dataCar, car, data_jt03, jt03);
        exit(EXIT_FAILURE);
    } else if (fileErr == 2) {
        free_memory(3, car, data_jt03, jt03);
        exit(EXIT_FAILURE);
    } else if (fileErr == 3) {
        free_memory(3, dataCar, car, jt03);
        exit(EXIT_FAILURE);
    }
    int ndof = nys * ndofysla;  // общее число степеней свободы
    // глобальная матрица жесткости kglb[ndof][ndof]
    double *dataKGLB = (double *)calloc(ndof * ndof, sizeof(double));
    double **kglb = (double **)calloc(ndof, sizeof(double *));
    for (int i = 0; i < ndof; i++) {
        kglb[i] = dataKGLB + i * ndof;
    }
    if (kglb == NULL) {
        free_memory(5, kglb, dataCar, car, data_jt03, jt03);
        exit(1);
    }
    u = (double *)malloc(ndof * sizeof(double));  // массив перемещений узлов
    if (u == NULL) {
        free_memory(6, kglb, dataCar, car, data_jt03, jt03, u);
        exit(1);
    }
    double *r = (double *)malloc(ndof * sizeof(double));  // массив нагрузок
    // массив x (рабочий LDLT)
    double *x = (double *)malloc(ndof * sizeof(double));
    // расчет матрицы лок. жесткости и добавление ее в глоб. матрицу
    AssembleLocalStiffnessToGlobal(kglb, jt03, car, nelem, e, h, puas, ndofysla);
    int lenNodePres = 0, lenNodeZakrU = 0, lenNodeZakrV = 0;
    int *nodePres = NULL;   // массив нагруженных узлов
    int *nodeZakrU = NULL;  // массив закрепленных узлов по X
    int *nodeZakrV = NULL;  // массив закрепленных узлов по Y
    // нахождение закрепленных и нагруженных узлов
    FillConstrainedLoadedNodes(&nodePres, &lenNodePres, &nodeZakrU, &lenNodeZakrU, &nodeZakrV, &lenNodeZakrV,
                               car, nys);
    SetLoadVector(r, lenNodePres, nodePres, ndofysla, ndof,
                  LOAD);  // задаем вектор нагрузок
    MakeConstrained(nodeZakrV, lenNodeZakrV, kglb,
                    ndofysla);  // задаем закрепления по V
    MakeConstrained(nodeZakrU, lenNodeZakrU, kglb,
                    ndofysla);  // задаем закрепления по U
    // решение СЛАУ методом разложения в LDLT
    bool_t ierr = solveLinearSystemLDLT(kglb, u, r, x, ndof);
    if (ierr) {  // ошибка разложения в LDLT или диаагонального решения
        free_memory(12, nodePres, nodeZakrU, nodeZakrV, u, r, x, dataKGLB, kglb, dataCar, car, data_jt03,
                    jt03);
        exit(1);
    }
    // расчет деформаций, напряжений
    double *dataStrain = NULL;  // массив деформаций
    double **strain = NULL;
    makeDoubleMtrx(&dataStrain, &strain, 4, nelem);
    if (strain == NULL) {
        free_memory(13, strain, nodePres, nodeZakrU, nodeZakrV, u, r, x, dataKGLB, kglb, dataCar, car,
                    data_jt03, jt03);
        exit(1);
    }
    double *dataStress = NULL;  // массив напряжений
    makeDoubleMtrx(&dataStress, &stress, 4, nelem);
    if (stress == NULL) {
        free_memory(15, stress, dataStrain, strain, nodePres, nodeZakrU, nodeZakrV, u, r, x, dataKGLB, kglb,
                    dataCar, car, data_jt03, jt03);
        exit(1);
    }
    stressModel(ndofysla, nelem, jt03, car, e, puas, u, strain, stress);

    writeResult(outputFile, jt03, strain, stress, r, u, nelem, nys, ndof);

    drawMashForSolve(argc, argv);  // отрисовка модели, разбитой на КЭ

    free_memory(16, dataStress, stress, dataStrain, strain, nodePres, nodeZakrU, nodeZakrV, u, r, x, dataKGLB,
                kglb, dataCar, car, data_jt03, jt03);
}