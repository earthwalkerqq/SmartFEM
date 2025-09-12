#include "mtrx.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void makeDoubleMtrx(double **dataMtrx, double ***mtrx, int row, int col) {
    *dataMtrx = (double *)malloc(row * col * sizeof(double));
    if (*dataMtrx == NULL) {
        perror("Can't allocate memory for dataMtrx\n");
        exit(1);
    }
    *mtrx = (double **)malloc(row * sizeof(double *));
    if (*mtrx == NULL) {
        perror("Can't allocate memory for row pointers\n");
        free(*dataMtrx);
        exit(1);
    }
    for (int i = 0; i < row; i++) {
        (*mtrx)[i] = *dataMtrx + i * col;
    }
}

void makeIntegerMtrx(int **dataMtrx, int ***mtrx, int row, int col) {
    *dataMtrx = (int *)malloc(row * col * sizeof(int));
    if (*dataMtrx == NULL) {
        perror("Can't allocate memory for dataMtrx\n");
        exit(1);
    }
    *mtrx = (int **)malloc(row * sizeof(int *));
    if (*mtrx == NULL) {
        perror("Can't allocate memory for row pointers\n");
        free(*dataMtrx);
        exit(1);
    }
    for (int i = 0; i < row; i++) {
        (*mtrx)[i] = *dataMtrx + i * col;
    }
}

void free_memory(int count, ...) {
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; i++) {
        void *pnt = va_arg(args, void *);
        free(pnt);
    }
    va_end(args);
}