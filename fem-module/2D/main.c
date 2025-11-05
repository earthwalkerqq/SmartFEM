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
    const char *inputFile = (argc == 2) ? argv[1] : "../../materials/data-sample/52.stp";

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
    // Сначала находим закрепленные узлы, чтобы проверить их количество
    int lenNodePres = 0, lenNodeZakrU = 0, lenNodeZakrV = 0;
    int *nodePres = NULL;   // массив нагруженных узлов
    int *nodeZakrU = NULL;  // массив закрепленных узлов по X
    int *nodeZakrV = NULL;  // массив закрепленных узлов по Y
    // нахождение закрепленных и нагруженных узлов
    FillConstrainedLoadedNodes(&nodePres, &lenNodePres, &nodeZakrU, &lenNodeZakrU, &nodeZakrV, &lenNodeZakrV,
                               car, nys);
    
    // Проверка наличия достаточного количества закреплений
    if (lenNodeZakrU == 0 || lenNodeZakrV == 0) {
        fprintf(stderr, "Ошибка: недостаточно граничных условий. U_fixed=%d, V_fixed=%d\n", lenNodeZakrU, lenNodeZakrV);
        free_memory(8, nodePres, nodeZakrU, nodeZakrV, dataCar, car, data_jt03, jt03, dataKGLB);
        exit(EXIT_FAILURE);
    }
    
    fprintf(stderr, "Граничные условия: U_fixed=%d, V_fixed=%d\n", lenNodeZakrU, lenNodeZakrV);
    
    // расчет матрицы лок. жесткости и добавление ее в глоб. матрицу
    AssembleLocalStiffnessToGlobal(kglb, jt03, car, nelem, e, h, puas, ndofysla);
    
    SetLoadVector(r, lenNodePres, nodePres, ndofysla, ndof,
                  LOAD);  // задаем вектор нагрузок
    
    // Применяем закрепления - закрепляем только нужные компоненты
    // Для узлов, закрепленных по V (y==0): закрепляем только V компоненту (DOF = node*2 + 1)
    for (int i = 0; i < lenNodeZakrV; i++) {
        int node = nodeZakrV[i];
        int kdof = node * ndofysla + 1;  // V компонента (индекс 1)
        // Обнуляем строку и столбец, кроме диагонали, и устанавливаем большое значение на диагонали
        for (int j = 0; j < ndof; j++) {
            if (j != kdof) {
                kglb[kdof][j] = 0.0;
                kglb[j][kdof] = 0.0;
            }
        }
        kglb[kdof][kdof] = 1.e38;
    }
    
    // Для узлов, закрепленных по U (x==0): закрепляем только U компоненту (DOF = node*2 + 0)
    for (int i = 0; i < lenNodeZakrU; i++) {
        int node = nodeZakrU[i];
        int kdof = node * ndofysla + 0;  // U компонента (индекс 0)
        // Обнуляем строку и столбец, кроме диагонали, и устанавливаем большое значение на диагонали
        for (int j = 0; j < ndof; j++) {
            if (j != kdof) {
                kglb[kdof][j] = 0.0;
                kglb[j][kdof] = 0.0;
            }
        }
        kglb[kdof][kdof] = 1.e38;
    }
    
    // Обнуляем нагрузку на закрепленных степенях свободы
    for (int i = 0; i < lenNodeZakrV; i++) {
        int node = nodeZakrV[i];
        r[node * ndofysla + 1] = 0.0;
    }
    for (int i = 0; i < lenNodeZakrU; i++) {
        int node = nodeZakrU[i];
        r[node * ndofysla + 0] = 0.0;
    }
    
    // Проверка и исправление: убеждаемся, что все диагональные элементы ненулевые
    // Устанавливаем минимальное значение для всех нулевых диагональных элементов
    // Это необходимо для изолированных узлов или узлов без связей
    int zero_diag_count = 0;
    for (int i = 0; i < ndof; i++) {
        if (fabs(kglb[i][i]) < 1.0e-20) {
            zero_diag_count++;
            // Устанавливаем большое значение для предотвращения сингулярности
            // Используем значение, сравнимое с закреплениями, но меньше
            kglb[i][i] = 1.e30;
        }
    }
    if (zero_diag_count > 0) {
        fprintf(stderr, "Обнаружено и исправлено %d нулевых диагональных элементов\n", zero_diag_count);
    }
    
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