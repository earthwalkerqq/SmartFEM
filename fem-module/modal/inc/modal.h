#ifndef MODAL_H
#define MODAL_H

typedef struct {
	int numModes;
	int ndof;
} modal_info;

void print_modal_info(const modal_info *info);

// Assemble global stiffness K using existing FEM assembly (2D triangles)
int assemble_global_stiffness_from_mesh(const char *meshFile,
		double ***K_out, double **K_data_out,
		int *ndof_out,
		int *nNodes_out,
		int *nElem_out,
		double ***car_out,
		int ***jt03_out);

// Build lumped mass matrix M (diagonal) for 2D linear triangles
// rho: density, h: thickness
int build_lumped_mass(double **M_diag, int ndof, int nElem, int **jt03, double **car, double rho, double h);

// Compute first `numModes` eigenpairs for K x = lambda M x using inverse power iteration with CG solver
int compute_modal_eigenpairs(int numModes, double **K, double *M_diag, int ndof,
		double tol, int maxIter,
		double *eigenVals, double **eigenVecs);

#endif

