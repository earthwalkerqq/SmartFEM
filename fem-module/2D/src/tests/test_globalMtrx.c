#include <stdio.h>

#include "test_fem.h"

START_TEST(test_formatationGlobalMtrx_1) {
    // fixture
    int ndofysla = 2;
    double e = 2.1e5;
    double puas = 0.3;
    double h = 1.;
    int jt03[3][2] = {{1, 1}, {2, 3}, {3, 4}};
    double car[3][4] = {{0., 1000., 1000., 0.}, {0., 0., 200., 200.}, {0., 0., 0., 0.}};
    int nelem = 2;
    int ndof = 8;
    // calculate
    double *dataGEST = (double *)malloc(6 * 6 * sizeof(double));
    double **gest = NULL;
    makeDoubleMtrx(&dataGEST, &gest, 6, 6);
    double *dataKGLB = (double *)calloc(ndof * ndof, sizeof(double));
    double **kglb = (double **)calloc(ndof, sizeof(double *));
    for (int i = 0; i < ndof; i++) {
        kglb[i] = dataKGLB + i * ndof;
    }
    for (int ielem = 0; ielem < nelem; ielem++) {
        nodeNumber node = {
            jt03[0][ielem],
            jt03[1][ielem],
            jt03[2][ielem],
        };
        coord coord1 = {car[0][node.iys1 - 1], car[1][node.iys1 - 1]};
        coord coord2 = {car[0][node.iys2 - 1], car[1][node.iys2 - 1]};
        coord coord3 = {car[0][node.iys3 - 1], car[1][node.iys3 - 1]};
        planeElement(coord1, coord2, coord3, e, h, puas, gest);
        assemblyGlobMatr(ndofysla, node, gest, kglb);
    }
    double resMtrx[8][8] = {
        {225000., 0., -23076.923077, 34615.384615, 0., -75000., -201923.076923, 40384.615385},
        {0., 585000., 40384.615385, -8076.923077, -75000., 0., 34615.384615, -576923.076923},
        {-23076.923077, 40384.615385, 225000., -75000., -201923.076923, 34615.384615, 0., 0.},
        {34615.384615, -8076.923077, -75000., 585000., 40384.615385, -576923.076923, 0., 0.},
        {0., -75000., -201923.076923, 40384.615385, 225000., 0., -23076.923077, 34615.384615},
        {-75000., 0., 34615.384615, -576923.076923, 0., 585000., 40384.615385, -8076.923077},
        {-201923.076923, 34615.384615, 0., 0., -23076.923077, 40384.615385, 225000., -75000.},
        {40384.615385, -576923.076923, 0., 0., 34615.384615, -8076.923077, -75000., 585000.}};
    for (int row = 0; row < ndof; row++) {
        for (int col = 0; col < ndof; col++) {
            ck_assert_double_ge(kglb[row][col], resMtrx[row][col] - 50);
            ck_assert_double_le(kglb[row][col], resMtrx[row][col] + 50);
        }
    }
    free_memory(4, dataGEST, dataKGLB, gest, kglb);
}
END_TEST

START_TEST(test_formatationGlobalMtrx_2) {
    coord coord1 = {0., 0.};
    coord coord2 = {20., 0.};
    coord coord3 = {20., 20.};
    int ndofysla = 2;
    double e = 2.1e5;
    double puas = 0.3;
    double h = 1.;
    int jt03[3][2] = {{1, 1}, {2, 3}, {3, 4}};
    double *dataGEST = (double *)malloc(6 * 6 * sizeof(double));
    double **gest = NULL;
    makeDoubleMtrx(&dataGEST, &gest, 6, 6);
    double *dataKGLB = (double *)malloc(6 * 6 * sizeof(double));
    double **kglb = NULL;
    makeDoubleMtrx(&dataKGLB, &kglb, 6, 6);
    nodeNumber node = {jt03[0][0], jt03[1][0], jt03[2][0]};
    planeElement(coord1, coord2, coord3, e, h, puas, gest);
    assemblyGlobMatr(ndofysla, node, gest, kglb);
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 6; col++) {
            ck_assert_double_eq(gest[row][col], kglb[row][col]);
        }
    }
    free_memory(4, dataGEST, dataKGLB, gest, kglb);
}
END_TEST

Suite *test_formatationGlobalMtrx(void) {
    Suite *s = suite_create("formatationGlobalMtrx");
    TCase *tc = tcase_create(" test_globalMtrx ");
    tcase_add_test(tc, test_formatationGlobalMtrx_1);
    tcase_add_test(tc, test_formatationGlobalMtrx_2);
    suite_add_tcase(s, tc);
    return s;
}