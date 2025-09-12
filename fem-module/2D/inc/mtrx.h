#ifndef MTRX_H
#define MTRX_H

#include "fem.h"

void makeDoubleMtrx(double **dataMtrx, double ***mtrx, int row, int col);
void makeIntegerMtrx(int **dataMtrx, int ***mtrx, int row, int col);

void free_memory(int count, ...);

#endif