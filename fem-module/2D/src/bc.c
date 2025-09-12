#include "bc.h"

#include <stdlib.h>

// функция заполнения массивов закрепленных узлов и массив нагруженных узлов
void FillConstrainedLoadedNodes(int **nodePres, int *lenNodePres, int **nodeZakrU, int *lenNodeZakrU,
                                int **nodeZakrV, int *lenNodeZakrV, double **car, int nys) {
    for (int i = 0; i < nys; i++) {
        if ((int)car[0][i] == 100) {
            (*lenNodePres)++;
            if (!(*lenNodePres)) {
                *nodePres = (int *)malloc(sizeof(int));
                (*nodePres)[*lenNodePres] = i;
            } else {
                *nodePres = (int *)realloc(*nodePres, *lenNodePres * sizeof(int));
                (*nodePres)[*lenNodePres - 1] = i;
            }
        }
        if (!(int)car[0][i]) {
            (*lenNodeZakrU)++;
            if (!(*lenNodeZakrU)) {
                *nodeZakrU = (int *)malloc(sizeof(int));
                (*nodeZakrU)[*lenNodeZakrU] = i;
            } else {
                *nodeZakrU = (int *)realloc(*nodeZakrU, *lenNodeZakrU * sizeof(int));
                (*nodeZakrU)[*lenNodeZakrU - 1] = i;
            }
        }
        if (!(int)car[1][i]) {
            (*lenNodeZakrV)++;
            if (!(*lenNodeZakrV)) {
                *nodeZakrV = (int *)malloc(sizeof(int));
                (*nodeZakrV)[*lenNodeZakrV] = i;
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
    for (int i = 0; i < lenNodePres; i++) {
        r[nodePres[i] * ndofysla] = load / lenNodePres;
    }
}