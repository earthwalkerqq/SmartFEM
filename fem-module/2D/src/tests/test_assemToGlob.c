#include "test_fem.h"

START_TEST(test_assembToBlobMtrx) {
    // fixture
    int ndofysla = 2;
    double e = 2.1e5;
    double puas = 0.3;
    double h = 1.;
    int jt03[3][2] = {{1, 1}, {2, 3}, {3, 4}};
    double car[3][4] = {{0., 1000., 1000., 0.}, {0., 0., 200., 200.}, {0., 0., 0., 0.}};
    int nelem = 2;
    int ndof = 8;
    double *dataGEST = (double *)malloc(6 * 6 * sizeof(double));
    double **gest = NULL;
    makeDoubleMtrx(&dataGEST, &gest, 6, 6);
    double *dataKGLB = (double *)calloc(ndof * ndof, sizeof(double));
    double **kglb = (double **)calloc(ndof, sizeof(double *));
    for (int i = 0; i < ndof; i++) {
        kglb[i] = dataKGLB + i * ndof;
    }
    // calculate
    AssembleLocalStiffnessToGlobal(kglb, (int **)jt03, (double **)car, nelem, e, h, puas, ndofysla);
    double *dataResMtrx = (double *)calloc(ndof * ndof, sizeof(double));
    double **resMtrx = (double **)calloc(ndof, sizeof(double *));
    for (int i = 0; i < ndof; i++) {
        resMtrx[i] = dataResMtrx + i * ndof;
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
        assemblyGlobMatr(ndofysla, node, gest, resMtrx);
    }
    for (int row = 0; row < ndof; row++) {
        for (int col = 0; col < ndof; col++) {
            ck_assert_double_ge(kglb[row][col], resMtrx[row][col] - 50);
            ck_assert_double_le(kglb[row][col], resMtrx[row][col] + 50);
        }
    }
    free_memory(6, dataGEST, dataKGLB, dataResMtrx, gest, kglb, resMtrx);
}
END_TEST

Suite *test_assemblGlobalMtrx(void) {
    Suite *s = suite_create("testAssemblGlobalMtrx");
    TCase *tc = tcase_create(" test_assemblToGlobalMtrx ");
    tcase_add_test(tc, test_assembToBlobMtrx);
    suite_add_tcase(s, tc);
    return s;
}