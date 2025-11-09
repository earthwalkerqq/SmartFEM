#include "bc.h"

#include <stdlib.h>
#include <stdio.h>

// функция заполнения массивов закрепленных узлов и массив нагруженных узлов
// car имеет размер 6 x nys: car[0]=x, car[1]=y, car[2]=z, car[3]=u_flag, car[4]=v_flag, car[5]=load_flag
void FillConstrainedLoadedNodes(int **nodePres, int *lenNodePres, int **nodeZakrU, int *lenNodeZakrU,
                                int **nodeZakrV, int *lenNodeZakrV, double **car, int nys) {
    for (int i = 0; i < nys; i++) {
        // Проверяем нагрузку (car[5][i] == 100 означает нагрузку)
        if ((int)car[5][i] == 100) {
            (*lenNodePres)++;
            if (*lenNodePres == 1) {
                *nodePres = (int *)malloc(sizeof(int));
                (*nodePres)[0] = i;
            } else {
                *nodePres = (int *)realloc(*nodePres, *lenNodePres * sizeof(int));
                (*nodePres)[*lenNodePres - 1] = i;
            }
        }
        // Проверяем закрепление по U (car[3][i] == 0 означает закрепление)
        if ((int)car[3][i] == 0) {
            (*lenNodeZakrU)++;
            if (*lenNodeZakrU == 1) {
                *nodeZakrU = (int *)malloc(sizeof(int));
                (*nodeZakrU)[0] = i;
            } else {
                *nodeZakrU = (int *)realloc(*nodeZakrU, *lenNodeZakrU * sizeof(int));
                (*nodeZakrU)[*lenNodeZakrU - 1] = i;
            }
        }
        // Проверяем закрепление по V (car[4][i] == 0 означает закрепление)
        if ((int)car[4][i] == 0) {
            (*lenNodeZakrV)++;
            if (*lenNodeZakrV == 1) {
                *nodeZakrV = (int *)malloc(sizeof(int));
                (*nodeZakrV)[0] = i;
            } else {
                *nodeZakrV = (int *)realloc(*nodeZakrV, *lenNodeZakrV * sizeof(int));
                (*nodeZakrV)[*lenNodeZakrV - 1] = i;
            }
        }
    }
}

void MakeConstrained(int *nodeZakr, int lenNodeZakr, double **kglb, int ndofysla) {
    for (int i = 0; i < lenNodeZakr; i++) {
        int kdof = nodeZakr[i] * ndofysla;
        kglb[kdof][kdof] += 1.e38;
    }
}

void SetLoadVector(double *r, int lenNodePres, int *nodePres, int ndofysla, int ndof, float load) {
    for (int i = 0; i < ndof; i++) {
        r[i] = 0.;
    }
    
    // Пытаемся прочитать нагрузки из файла loads.txt
    // Ищем файл в нескольких местах
    const char *loadsPaths[] = {
        "loads.txt",
        "../build/loads.txt",
        "../../build/loads.txt",
        "./build/loads.txt",
        NULL
    };
    
    FILE *loadsFile = NULL;
    for (int i = 0; loadsPaths[i] != NULL; i++) {
        loadsFile = fopen(loadsPaths[i], "r");
        if (loadsFile != NULL) {
            break;
        }
    }
    
    if (loadsFile != NULL) {
        int nodeId;
        double fx, fy;
        int loadsRead = 0;
        // Читаем нагрузки из файла
        while (fscanf(loadsFile, "%d%lf%lf", &nodeId, &fx, &fy) == 3) {
            // nodeId в файле начинается с 1, в массиве - с 0
            int nodeIdx = nodeId - 1;
            if (nodeIdx >= 0 && nodeIdx < ndof / ndofysla) {
                // Применяем нагрузку к узлу
                r[nodeIdx * ndofysla + 0] = fx;  // Fx
                r[nodeIdx * ndofysla + 1] = fy;  // Fy
                loadsRead++;
            }
        }
        fclose(loadsFile);
        if (loadsRead > 0) {
            fprintf(stderr, "Загружено %d нагрузок из файла loads.txt\n", loadsRead);
        }
    } else {
        // Если файл не найден, используем старую логику (равномерное распределение нагрузки)
        if (lenNodePres > 0) {
            fprintf(stderr, "Файл loads.txt не найден, используем старую логику распределения нагрузки\n");
            for (int i = 0; i < lenNodePres; i++) {
                r[nodePres[i] * ndofysla] = load / lenNodePres;
            }
        }
    }
}