#ifndef LDLT_H
#define LDLT_H

#include "fem.h"

bool_t matrLDLT(int ndof, double **kglb);
void direktLDLT(int ndof, double **kglb, double *x, double *r);
bool_t diagLDLT(int ndof, double **kglb, double *x);
void rechLDLT(int ndof, double **kglb, double *u, double *x);

bool_t solveLinearSystemLDLT(double **kglb, double *u, double *r, double *x, int ndof);

#endif