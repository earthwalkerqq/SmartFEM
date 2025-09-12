#ifndef FEM_H
#define FEM_H

typedef enum { FALSE = 0, TRUE } bool_t;

typedef struct coord_t coord_t;
typedef struct nodeNumber_t nodeNumber_t;
typedef struct deformComp_t deformComp_t;
typedef struct stressComp_t stressComp_t;

void planeElement(coord_t coord1, coord_t coord2, coord_t coord3, double e, double h, double puas,
                  double **gest);

double **formationDeformMtrx(double **deformMtrx, coord_t coord1, coord_t coord2, coord_t coord3, double a2);
double **formationElastMtrx(double **elastMtrx, double e, double puas);
void stressPlanElem(coord_t coord1, coord_t coord2, coord_t coord3, double e, double puas,
                    double **deformMtrx, double **strsMatr);

void stressModel(int ndofysla, int nelem, int **jt03, double **car, double e, double puas, double *u,
                 double **strain, double **stress);

void AssembleLocalStiffnessToGlobal(double **kglb, int **jt03, double **car, int nelem, double e, double h,
                                    double puas, int ndofysla);
void assemblyGlobMatr(int ndofysla, nodeNumber_t node, double **gest, double **kglb);

#endif