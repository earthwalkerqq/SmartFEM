#ifndef BC_H
#define BC_H

void FillConstrainedLoadedNodes(int **nodePres, int *lenNodePres, int **nodeZakrU, int *lenNodeZakrU,
                                int **nodeZakrV, int *lenNodeZakrV, double **car, int nys);

void MakeConstrained(int *nodeZakr, int lenNodeZakr, double **kglb, int ndofysla);
void SetLoadVector(double *r, int lenNodePres, int *nodePres, int ndofysla, int ndof, float load);

#endif