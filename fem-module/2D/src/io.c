#include <stdio.h>

#include "io.h"
#include "mtrx.h"

bool_t readFromFile(char *filename, int *nys, double **dataCar, double ***car, int *nelem, int **data_jt03,
                    int ***jt03) {
    bool_t err = FALSE;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        err = TRUE;
    } else {
        fscanf(file, "%d", nys);
        makeDoubleMtrx(dataCar, car, 3, *nys);  // массив координат узлов элемента
    }
    if (*car == NULL) {
        err = TRUE;
    }
    for (int i = 0; i < *nys; i++) {
        fscanf(file, "%lf%lf%lf", &(*car)[0][i], &(*car)[1][i], &(*car)[2][i]);
    }
    fscanf(file, "%d", nelem);
    makeIntegerMtrx(data_jt03, jt03, 3, *nelem);  // массив номеров узлов элемента
    if (*jt03 == NULL) {
        err = TRUE;
    }
    for (int i = 0; i < *nelem; i++) {
        fscanf(file, "%d%d%d", &(*jt03)[0][i], &(*jt03)[1][i], &(*jt03)[2][i]);
    }

    fclose(file);
    return err;
}

bool_t writeResult(char *filename, int **jt03, double **strain, double **stress, double *r, double *u,
                   int nelem, int nys, int ndof) {
    bool_t error = FALSE;
    FILE *file = fopen(filename, "w");
    if (!file) {
        error = TRUE;
    } else {
        fprintf(file, "Число элементов - %d\n", nelem);
        fprintf(file, "Число узлов - %d\n", nys);
        fprintf(file, "Число степеней свободы - %d\n", ndof);
        fprintf(file, "\n");
        fprintf(file, "Вектор нагрузок\n");
        int index = 1;
        for (int i = 0; i <= ndof; i += 2) {
            fprintf(file, "       ru%d       %12.4e       rv%d       %12.4e\n", index, r[i], index, r[i + 1]);
            index++;
        }
        fprintf(file, "\n");
        fprintf(file, "Результат расчета перемещений\n");
        index = 1;
        for (int i = 0; i <= ndof; i += 2) {
            fprintf(file, "       u%d       %12.4e       v%d       %12.4e\n", index, u[i], index, u[i + 1]);
            index++;
        }
        fprintf(file, "\n");
        fprintf(file, "Результат расчета деформаций, напряжений\n");
        for (int j = 0; j < nelem; j++) {
            int ielem = jt03[0][j];
            fprintf(file, "       %d       ", ielem);
            for (int i = 0; i < 4; i++) {
                fprintf(file, "       %12.4f", strain[i][ielem - 1]);
            }
            fprintf(file, "       |");
            for (int i = 0; i < 4; i++) {
                fprintf(file, "       %12.4f", stress[i][ielem - 1]);
            }
            fprintf(file, "\n");
        }
        fclose(file);
    }
    return error;
}