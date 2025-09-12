#include "test_fem.h"

START_TEST(test_elastMtrx_formation) {
    // fixture
    double puas = 0.3;
    double e = 2.1e5;
    // calculate
    double *dataElastMtrx = (double *)malloc(3 * 3 * sizeof(double));
    double **elastMtrx = (double **)malloc(3 * sizeof(double *));
    for (int i = 0; i < 3; i++) {
        elastMtrx[i] = dataElastMtrx + i * 3;
    }
    elastMtrx = formationElastMtrx(elastMtrx, e, puas);
    double koef = e / (1. - puas * puas);
    double resultElastMrtx[3][3] = {
        {koef, puas * koef, 0.}, {puas * koef, koef, 0.}, {0., 0., ((1. - puas) / 2) * koef}};
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            ck_assert_double_eq(elastMtrx[row][col], resultElastMrtx[row][col]);
        }
    }
    free_memory(2, dataElastMtrx, elastMtrx);
}
END_TEST

Suite *test_formatationElastMtrx(void) {
    Suite *s = suite_create("formatationElastMtrx");
    TCase *tc = tcase_create(" test_elastMtrx ");
    tcase_add_test(tc, test_elastMtrx_formation);
    suite_add_tcase(s, tc);
    return s;
}