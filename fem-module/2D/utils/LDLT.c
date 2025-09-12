#include "LDLT.h"

#include <math.h>
#include <stdio.h>

// разложение матрицы глобальной матрицы жесткости kglb[ndof][ndof] в LDLT
bool_t matrLDLT(int ndof, double **kglb) {
    double diag;
    int i = 0;
    bool_t ierr = FALSE;
    double sum = kglb[i][i];
    if (fabs(sum) < 1.0e-20) {
        perror("Разложение невозможно. Нулевой диагольный элемент");
        return TRUE;
    }
#pragma omp parallel shared(kglb, sum, ndof)
    {
#pragma omp for
        for (int j = 1; j < ndof; j++) {
            kglb[j][0] /= sum;
            kglb[0][j] = 0.;
        }
    }
    for (i = 1; i < ndof; i++) {
        diag = kglb[i][i];
        for (int j = 0; j < i; j++) {
            diag -= kglb[j][j] * (kglb[i][j] * kglb[i][j]);
        }
        kglb[i][i] = diag;
        if (fabs(diag) < 1.e-20) {
            perror("Разложение невозможно. Нулевой диагональный элемент\n");
            return TRUE;
        }
#pragma omp parallel shared(kglb, i, ndof, diag) private(sum)
        {
#pragma omp for
            for (int k = i + 1; k < ndof; k++) {
                sum = kglb[k][i];
                for (int j = 0; j < i; j++) {
                    sum -= kglb[j][j] * (kglb[k][j] * kglb[i][j]);
                }
                kglb[k][i] = sum / diag;
                kglb[i][k] = 0.;
            }
        }
    }
    return ierr;
}

// прямая подстановка. Решение L*X=R
void direktLDLT(int ndof, double **kglb, double *x, double *r) {
    // r - массив нагрузок; x - рабочий массив
    // разложение симметричной матрицы kglb в LDLT
    double sum;
    x[0] = r[0];
    for (int i = 1; i < ndof; i++) {
        sum = 0.0;
#pragma omp parallel shared(kglb, i) reduction(+ : sum)
        {
#pragma omp for
            for (int j = 0; j < i; j++) {
                sum += kglb[i][j] * x[j];
            }
        }
        x[i] = r[i] - sum;
    }
}

// Диагональное решение. Решение D*Y=X
bool_t diagLDLT(int ndof, double **kglb, double *x) {
    bool_t ierr = FALSE;
    double diag;
#pragma omp parallel shared(kglb, ndof)
    {
#pragma omp for
        for (int i = 0; i < ndof; i++) {
            diag = kglb[i][i];
            if (fabs(diag) < 1.0e-20) {
                perror("Решение невозможно. Нулевой диагональный элемент");
                ierr = TRUE;
            }
            x[i] /= diag;
        }
    }
    return ierr;
}

// обратная подстановка. Решение LT*U=X
void rechLDLT(int ndof, double **kglb, double *u, double *x) {
    // u - массив перемещений
    double sum;
    u[ndof - 1] = x[ndof - 1];
    for (int i = ndof - 2; i >= 0; i--) {
        sum = 0.0;
        for (int j = i + 1; j < ndof; j++) {
            sum += kglb[j][i] * u[j];
        }
        u[i] = x[i] - sum;
    }
}

bool_t solveLinearSystemLDLT(double **kglb, double *u, double *r, double *x, int ndof) {
    bool_t ierr = matrLDLT(ndof, kglb);
    if (!ierr) {
        direktLDLT(ndof, kglb, x, r);
        ierr = diagLDLT(ndof, kglb, x);
        if (!ierr) {
            rechLDLT(ndof, kglb, u, x);
        }
    }
    return ierr;
}