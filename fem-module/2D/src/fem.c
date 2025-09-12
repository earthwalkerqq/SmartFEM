#include "fem.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "mtrx.h"

struct coord_t {
    double x;
    double y;
};

struct nodeNumber_t {
    int iys1;
    int iys2;
    int iys3;
};

struct deformComp_t {
    double ex;
    double ey;
    double exy;
};

struct stressComp_t {
    double sx;
    double sy;
    double sxy;
};

// расчет матрицы деформаций
double **formationDeformMtrx(double **deformMtrx, coord_t coord1, coord_t coord2, coord_t coord3, double a2) {
    double a;
    a = (a2 > 0.) ? 1 / (a2) : 1.;
    deformMtrx[0][0] = a * (coord2.y - coord3.y);
    deformMtrx[0][1] = 0;
    deformMtrx[0][2] = a * (coord3.y - coord1.y);
    deformMtrx[0][3] = 0;
    deformMtrx[0][4] = a * (coord1.y - coord2.y);
    deformMtrx[0][5] = 0;
    deformMtrx[1][0] = 0;
    deformMtrx[1][1] = a * (coord3.x - coord2.x);
    deformMtrx[1][2] = 0;
    deformMtrx[1][3] = a * (coord1.x - coord3.x);
    deformMtrx[1][4] = 0;
    deformMtrx[1][5] = a * (coord2.x - coord1.x);
    deformMtrx[2][0] = a * (coord3.x - coord2.x);
    deformMtrx[2][1] = a * (coord2.y - coord3.y);
    deformMtrx[2][2] = a * (coord1.x - coord3.x);
    deformMtrx[2][3] = a * (coord3.y - coord1.y);
    deformMtrx[2][4] = a * (coord2.x - coord1.x);
    deformMtrx[2][5] = a * (coord1.y - coord2.y);
    return deformMtrx;
}

// расчет матрицы упругости
double **formationElastMtrx(double **elastMtrx, double e, double puas) {
    double ekoef = e / (1 - puas * puas);
    elastMtrx[0][0] = ekoef;
    elastMtrx[0][1] = ekoef * puas;
    elastMtrx[0][2] = 0;
    elastMtrx[1][0] = ekoef * puas;
    elastMtrx[1][1] = ekoef;
    elastMtrx[1][2] = 0;
    elastMtrx[2][0] = 0;
    elastMtrx[2][1] = 0;
    elastMtrx[2][2] = ekoef * (1 - puas) / 2;
    return elastMtrx;
}

// расчет матрицы напряжений
void stressPlanElem(coord_t coord1, coord_t coord2, coord_t coord3, double e, double puas,
                    double **deformMtrx, double **strsMatr) {
    // формирование матрицы упругости elastMtrx[3][3]
    double *dataElastMtrx = (double *)malloc(3 * 3 * sizeof(double));
    double **elastMtrx = NULL;
    makeDoubleMtrx(&dataElastMtrx, &elastMtrx, 3, 3);
    // заполнение матрицы упругости
    elastMtrx = formationElastMtrx(elastMtrx, e, puas);
    // заполнение матрицы деформаций deformMtrx[3][6]
    double a2 = coord2.x * coord3.y - coord3.x * coord2.y - coord1.x * coord3.y + coord1.y * coord3.x +
                coord1.x * coord2.y - coord1.y * coord2.x;
    deformMtrx = formationDeformMtrx(deformMtrx, coord1, coord2, coord3, a2);
    // заполнение матрицы напряжений
    // strsMatr[3][6]=elastMtrx[3][3]*deformMtrx[3][6]
    double sum;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 6; j++) {
            sum = 0.;
            for (int k = 0; k < 3; k++) {
                sum += elastMtrx[i][k] * deformMtrx[k][j];
            }
            strsMatr[i][j] = sum;
        }
    }
    free_memory(2, dataElastMtrx, elastMtrx);
}

// заполнение локальной матрицы жесткости треугольного конечного элемента
void planeElement(coord_t coord1, coord_t coord2, coord_t coord3, double e, double h, double puas,
                  double **gest) {
    double sum;
    // формирование матрица деформаций deformMtrx[3][6]
    double *dataDefMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **deformMtrx = NULL;
    makeDoubleMtrx(&dataDefMtrx, &deformMtrx, 3, 6);
    // заполнение матрицы деформаций
    double a2 = coord2.x * coord3.y - coord3.x * coord2.y - coord1.x * coord3.y + coord1.y * coord3.x +
                coord1.x * coord2.y - coord1.y * coord2.x;
    // формирование матрицы напряжений strsMatr[3][6]
    double *dataStrsMatr = (double *)malloc(3 * 6 * sizeof(double));
    double **strsMatr = NULL;
    makeDoubleMtrx(&dataStrsMatr, &strsMatr, 3, 6);
    // заполнение матрицы напряжений
    stressPlanElem(coord1, coord2, coord3, e, puas, deformMtrx, strsMatr);
    double vol = h * a2 * 0.5;
    // вычисление локальной матрицы жесткости
    // gest[6][6]=deformMtrx(trans)[6][3]*strsMatr[3][6];
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            sum = 0.;
            for (int k = 0; k < 3; k++) {
                sum += deformMtrx[k][i] * strsMatr[k][j];
            }
            gest[i][j] = sum * vol;
        }
    }
    // освобождаем память
    free_memory(4, dataDefMtrx, dataStrsMatr, deformMtrx, strsMatr);
}

// расчет матрицы лок. жесткости и добавление ее в глоб. матрицу
void AssembleLocalStiffnessToGlobal(double **kglb, int **jt03, double **car, int nelem, double e, double h,
                                    double puas, int ndofysla) {
    // локальная матрица жесткости gest[6][6]
    double *dataGEST = NULL;
    double **gest = NULL;
    makeDoubleMtrx(&dataGEST, &gest, 6, 6);
    for (int ielem = 0; ielem < nelem; ielem++) {
        nodeNumber_t node = {
            jt03[0][ielem],
            jt03[1][ielem],
            jt03[2][ielem],
        };
        coord_t coord1 = {car[0][node.iys1 - 1], car[1][node.iys1 - 1]};
        coord_t coord2 = {car[0][node.iys2 - 1], car[1][node.iys2 - 1]};
        coord_t coord3 = {car[0][node.iys3 - 1], car[1][node.iys3 - 1]};
        planeElement(coord1, coord2, coord3, e, h, puas, gest);
        assemblyGlobMatr(ndofysla, node, gest, kglb);
    }
    free_memory(2, dataGEST, gest);
}

// функция сборки глобальной матрицы жесткости kglb[ndof][ndof]
void assemblyGlobMatr(int ndofysla, nodeNumber_t node, double **gest, double **kglb) {
    int il, jl, ig, jg;  // начальные позиции в лок. и глоб. матрицах
    int iblok, jblok;    // добавочные коэф. к позициям матриц
    int nys[3];
    nys[0] = node.iys1 - 1;
    nys[1] = node.iys2 - 1;
    nys[2] = node.iys3 - 1;
    for (int iy = 0; iy < 3; iy++) {
        for (int jy = 0; jy < 3; jy++) {
            il = iy * ndofysla;
            jl = jy * ndofysla;
            ig = nys[iy] * ndofysla;
            jg = nys[jy] * ndofysla;
            for (iblok = 0; iblok < ndofysla; iblok++) {
                for (jblok = 0; jblok < ndofysla; jblok++) {
                    kglb[ig + iblok][jg + jblok] += gest[il + iblok][jl + jblok];
                }
            }
        }
    }
}

// расчет напряжений и деформаций в элементах модели
void stressModel(int ndofysla, int nelem, int **jt03, double **car, double e, double puas, double *u,
                 double **strain, double **stress) {
    // формирование матрицы деформаций deformMtrx[3][6]
    double *dataDeformMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **deformMtrx = (double **)malloc(3 * sizeof(double *));
    for (int i = 0; i < 3; i++) {
        deformMtrx[i] = dataDeformMtrx + i * 3;
    }
    // формирование матрицы напряжений strsMtrx[3][6]
    double *dataStrsMtrx = (double *)malloc(3 * 6 * sizeof(double));
    double **strsMtrx = (double **)malloc(3 * sizeof(double *));
    for (int i = 0; i < 3; i++) {
        strsMtrx[i] = dataStrsMtrx + i * 3;
    }
    // перемещения(uElement[6]), напряжения(eStress[3]) и деформации(eStrain[3])
    // одного элемента
    double *uElement = (double *)malloc(6 * sizeof(double));
    double *eStress = (double *)malloc(3 * sizeof(double));
    double *eStrain = (double *)malloc(3 * sizeof(double));
#pragma omp parallel shared(jt03, car, uElement, eStress, eStrain)
    {
#pragma omp for
        for (int ielem = 0; ielem < nelem; ielem++) {
            nodeNumber_t node = {jt03[0][ielem] - 1, jt03[1][ielem] - 1, jt03[2][ielem] - 1};
            coord_t coord1 = {car[0][node.iys1], car[1][node.iys1]};
            coord_t coord2 = {car[0][node.iys2], car[1][node.iys2]};
            coord_t coord3 = {car[0][node.iys3], car[1][node.iys3]};
            stressPlanElem(coord1, coord2, coord3, e, puas, deformMtrx, strsMtrx);
            uElement[0] = u[node.iys1 * ndofysla];
            uElement[1] = u[node.iys1 * ndofysla + 1];
            uElement[2] = u[node.iys2 * ndofysla];
            uElement[3] = u[node.iys2 * ndofysla + 1];
            uElement[4] = u[node.iys3 * ndofysla];
            uElement[5] = u[node.iys3 * ndofysla + 1];
            double sum;
            for (int i = 0; i < 3; i++) {
                sum = 0.;
                for (int j = 0; j < 6; j++) {
                    sum += strsMtrx[i][j] * uElement[j];
                }
                eStress[i] = sum;
                sum = 0.;
                for (int j = 0; j < 6; j++) {
                    sum += deformMtrx[i][j] * uElement[j];
                }
                eStrain[i] = sum;
            }
            deformComp_t dC = {eStrain[0], eStrain[1], eStrain[2]};
            strain[0][ielem] = dC.ex;
            strain[1][ielem] = dC.ey;
            strain[2][ielem] = dC.exy;
            strain[3][ielem] = sqrt(2.0) / 3.0 *
                               sqrt(dC.ex * dC.ey + (dC.ex - dC.ey) * (dC.ex - dC.ey) + dC.ey * dC.ey +
                                    1.5 * dC.exy * dC.exy);
            stressComp_t sC = {eStress[0], eStress[1], eStress[2]};
            stress[0][ielem] = sC.sx;
            stress[1][ielem] = sC.sy;
            stress[2][ielem] = sC.sxy;
            stress[3][ielem] = sqrt(sC.sx * sC.sx - sC.sx * sC.sy + sC.sy * sC.sy + 3. * sC.sxy * sC.sxy);
        }
    }
    free_memory(7, dataDeformMtrx, dataStrsMtrx, strsMtrx, deformMtrx, uElement, eStress, eStrain);
}