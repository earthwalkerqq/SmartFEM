#include <stdio.h>

#include "test_fem.h"

START_TEST(test_stressMtrx_formatation) {
    // fixture
    coord coord1 = {0., 0.};
    coord coord2 = {20., 0.};
    coord coord3 = {20., 20.};
    double e = 2.1e5;
    double puas = 0.3;
    // calculate
    double *dataStrsMxtr = (double *)malloc(3 * 6 * sizeof(double));
    double **strsMxtr = NULL;
    makeDoubleMtrx(&dataStrsMxtr, &strsMxtr, 3, 6);
    double *dataDeformMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **deformMtrx = NULL;
    makeDoubleMtrx(&dataDeformMtrx, &deformMtrx, 3, 6);
    double *dataElastMtrx = (double *)malloc(3 * 3 * sizeof(double));
    double **elastMtrx = NULL;
    makeDoubleMtrx(&dataElastMtrx, &elastMtrx, 3, 3);
    formationElastMtrx(elastMtrx, e, puas);
    stressPlanElem(coord1, coord2, coord3, e, puas, deformMtrx, strsMxtr);
    double *dataResMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **resMtrx = NULL;
    makeDoubleMtrx(&dataResMtrx, &resMtrx, 3, 6);
    double sum;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 6; j++) {
            sum = 0.;
            for (int k = 0; k < 3; k++) {
                sum += elastMtrx[i][k] * deformMtrx[k][j];
            }
            resMtrx[i][j] = sum;
        }
    }
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 6; col++) {
            ck_assert_double_eq(resMtrx[row][col], strsMxtr[row][col]);
        }
    }
    free_memory(8, dataDeformMtrx, dataElastMtrx, dataResMtrx, dataStrsMxtr, deformMtrx, elastMtrx, resMtrx,
                strsMxtr);
}
END_TEST

Suite *test_formatationStressMtrx(void) {
    Suite *s = suite_create("formatationStressMtrx");
    TCase *tc = tcase_create(" test_stressMtrx ");
    tcase_add_test(tc, test_stressMtrx_formatation);
    suite_add_tcase(s, tc);
    return s;
}