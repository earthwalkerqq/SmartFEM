#include "test_fem.h"

START_TEST(test_deformMtrx_formatation) {
    // fixture
    coord coord1 = {0., 0.};
    coord coord2 = {20., 0.};
    coord coord3 = {20., 20.};
    // calculate
    double *dataDeformMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **deformMxtr = (double **)malloc(3 * sizeof(double *));
    for (int i = 0; i < 3; i++) {
        deformMxtr[i] = dataDeformMtrx + i * 6;
    }

    double a2 = coord2.x * coord3.y - coord2.y * coord3.x + coord3.x * coord1.y - coord3.y * coord1.x +
                coord1.x * coord2.y - coord1.y * coord2.x;
    double a = (a2 > 0.) ? 1 / (a2) : 1.;
    deformMxtr = formationDeformMtrx(deformMxtr, coord1, coord2, coord3, a2);
    double resMtrx[3][6] = {{a * (-20.), 0., a * 20., 0., 0., 0.},
                            {0., 0., 0., a * (-20.), 0., a * 20.},
                            {0., a * (-20.), a * (-20.), a * 20., a * 20., 0.}};
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 6; col++) {
            ck_assert_double_eq(deformMxtr[row][col], resMtrx[row][col]);
        }
    }
    free_memory(2, dataDeformMtrx, deformMxtr);
}
END_TEST

Suite *test_formatationDeformMtrx(void) {
    Suite *s = suite_create("formatationDeformMtrx");
    TCase *tc = tcase_create(" test_deformMtrx ");
    tcase_add_test(tc, test_deformMtrx_formatation);
    suite_add_tcase(s, tc);
    return s;
}