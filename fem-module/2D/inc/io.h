#ifndef IO_H
#define IO_H

#include "fem.h"

bool_t readFromFile(char *filename, int *nys, double **dataCar, double ***car, int *nelem, int **data_jt03,
                    int ***jt03);
bool_t writeResult(char *filename, int **jt03, double **strain, double **stress, double *r, double *u,
                   int nelem, int nys, int ndof);

#endif