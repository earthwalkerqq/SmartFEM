#include <stdio.h>

#include "test_fem.h"

START_TEST(test_planeElem_formatation) {
    // fixture
    coord coord1 = {0., 0.};
    coord coord2 = {20., 0.};
    coord coord3 = {20., 20.};
    double e = 2.1e5;
    double puas = 0.3;
    double h = 1.;
    // calculate
    double *dataGEST = (double *)malloc(6 * 6 * sizeof(double));
    double **gest = NULL;
    makeDoubleMtrx(&dataGEST, &gest, 6, 6);
    planeElement(coord1, coord2, coord3, e, h, puas, gest);
    double *dataDeformMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **deformMtrx = NULL;
    makeDoubleMtrx(&dataDeformMtrx, &deformMtrx, 3, 6);
    double *dataStrsMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **strsMtrx = NULL;
    makeDoubleMtrx(&dataStrsMtrx, &strsMtrx, 3, 6);
    stressPlanElem(coord1, coord2, coord3, e, puas, deformMtrx, strsMtrx);
    double resMtrx[6][6] = {{1.154e5, 0., -1.154e5, 3.462e4, 0., -3.462e4},
                            {0., 4.038e4, 4.038e4, -4.038e4, -4.038e4, 0.},
                            {-1.154e5, 4.038e4, 1.558e5, -7.5e4, -4.038e4, 3.462e4},
                            {3.462e4, -4.038e4, -7.5e4, 1.558e5, 4.038e4, -1.154e5},
                            {0., -4.038e4, -4.038e4, 4.038e4, 4.038e4, 0.},
                            {-3.462e4, 0., 3.462e4, -1.154e5, 0., 1.154e5}};
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 6; col++) {
            ck_assert_double_ge(gest[row][col], resMtrx[row][col] - 50);
            ck_assert_double_le(gest[row][col], resMtrx[row][col] + 50);
        }
    }
    free_memory(6, dataDeformMtrx, dataGEST, dataStrsMtrx, deformMtrx, gest, strsMtrx);
}
END_TEST

Suite *test_formatationPlaneElem(void) {
    Suite *s = suite_create("formatationPlaneElem");
    TCase *tc = tcase_create(" test_planeElem ");
    tcase_add_test(tc, test_planeElem_formatation);
    suite_add_tcase(s, tc);
    return s;
}