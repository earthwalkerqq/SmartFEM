#include <stdio.h>
#include "modal.h"
// reuse 2D FEM IO and assembly
#include "io.h"
#include "fem.h"
#include <math.h>
#include <stdlib.h>

void print_modal_info(const modal_info *info) {
	printf("modes=%d, ndof=%d\n", info ? info->numModes : 0, info ? info->ndof : 0);
}

static void normalize_M(double *v, const double *M_diag, int ndof) {
	double dot = 0.0;
	for (int i = 0; i < ndof; i++) dot += M_diag[i] * v[i] * v[i];
	double invNorm = (dot > 0.0) ? 1.0 / sqrt(dot) : 1.0;
	for (int i = 0; i < ndof; i++) v[i] *= invNorm;
}

static double rayleigh_generalized(const double *v, double **K, const double *M_diag, int ndof) {
	double num = 0.0, den = 0.0;
	for (int i = 0; i < ndof; i++) {
		double Kv_i = 0.0;
		for (int j = 0; j < ndof; j++) Kv_i += K[i][j] * v[j];
		num += v[i] * Kv_i;
		den += M_diag[i] * v[i] * v[i];
	}
	return (den > 0.0) ? (num / den) : 0.0;
}

static void K_mul(const double **K, const double *x, double *y, int n) {
	for (int i = 0; i < n; i++) {
		double s = 0.0;
		for (int j = 0; j < n; j++) s += K[i][j] * x[j];
		y[i] = s;
	}
}

static void cg_solve(double **K, const double *b, double *x, int n, int maxIter, double tol) {
	for (int i = 0; i < n; i++) x[i] = 0.0;
	double *r = (double *)malloc(n * sizeof(double));
	double *p = (double *)malloc(n * sizeof(double));
	double *Ap = (double *)malloc(n * sizeof(double));
	// r = b - Kx (x=0 initially)
	for (int i = 0; i < n; i++) r[i] = b[i];
	for (int i = 0; i < n; i++) p[i] = r[i];
	double rsold = 0.0; for (int i = 0; i < n; i++) rsold += r[i] * r[i];
	for (int it = 0; it < maxIter; it++) {
		K_mul((const double **)K, p, Ap, n);
		double pAp = 0.0; for (int i = 0; i < n; i++) pAp += p[i] * Ap[i];
		if (fabs(pAp) < 1e-20) break;
		double alpha = rsold / pAp;
		for (int i = 0; i < n; i++) x[i] += alpha * p[i];
		for (int i = 0; i < n; i++) r[i] -= alpha * Ap[i];
		double rsnew = 0.0; for (int i = 0; i < n; i++) rsnew += r[i] * r[i];
		if (sqrt(rsnew) < tol) break;
		double beta = rsnew / rsold;
		for (int i = 0; i < n; i++) p[i] = r[i] + beta * p[i];
		rsold = rsnew;
	}
	free(Ap); free(p); free(r);
}

int assemble_global_stiffness_from_mesh(const char *meshFile,
		double ***K_out, double **K_data_out,
		int *ndof_out,
		int *nNodes_out,
		int *nElem_out,
		double ***car_out,
		int ***jt03_out) {
	int nys, nelem;
	double *dataCar = NULL; double **car = NULL;
	int *data_jt03 = NULL; int **jt03 = NULL;
	if (readFromFile((char *)meshFile, &nys, &dataCar, &car, &nelem, &data_jt03, &jt03)) {
		return 1;
	}
	if (nys <= 0 || nelem <= 0 || car == NULL || jt03 == NULL) {
		return 1;
	}
	const int ndofysla = 2;
	int ndof = nys * ndofysla;
	double *dataK = (double *)calloc(ndof * ndof, sizeof(double));
	if (!dataK) return 1;
	double **K = (double **)calloc(ndof, sizeof(double *));
	if (!K) { free(dataK); return 1; }
	for (int i = 0; i < ndof; i++) K[i] = dataK + i * ndof;
	const double h = 1.0; const double e = 2.1e5; const double nu = 0.3;
	AssembleLocalStiffnessToGlobal(K, jt03, car, nelem, e, h, nu, ndofysla);
	*K_out = K; *K_data_out = dataK; *ndof_out = ndof; *nNodes_out = nys; *nElem_out = nelem; *car_out = car; *jt03_out = jt03;
	// leak dataCar/data_jt03 to keep car/jt03 valid (managed by caller)
	return 0;
}

int build_lumped_mass(double **M_diag_out, int ndof, int nElem, int **jt03, double **car, double rho, double h) {
	if (!jt03 || !car || nElem <= 0 || ndof <= 0) return 1;
	double *M_diag = (double *)calloc(ndof, sizeof(double));
	if (!M_diag) return 1;
	int nNodes = ndof / 2;
	for (int e = 0; e < nElem; e++) {
		int n1 = jt03[0][e] - 1, n2 = jt03[1][e] - 1, n3 = jt03[2][e] - 1;
		if (n1 < 0 || n1 >= nNodes || n2 < 0 || n2 >= nNodes || n3 < 0 || n3 >= nNodes) {
			free(M_diag);
			return 1;
		}
		double x1 = car[0][n1], y1 = car[1][n1];
		double x2 = car[0][n2], y2 = car[1][n2];
		double x3 = car[0][n3], y3 = car[1][n3];
		double A = 0.5 * fabs(x2 * y3 - x3 * y2 - x1 * y3 + y1 * x3 + x1 * y2 - y1 * x2);
		double me = rho * h * A / 3.0; // per node (lumped)
		int idx1u = 2 * n1, idx1v = 2 * n1 + 1;
		int idx2u = 2 * n2, idx2v = 2 * n2 + 1;
		int idx3u = 2 * n3, idx3v = 2 * n3 + 1;
		if (idx1u >= ndof || idx1v >= ndof || idx2u >= ndof || idx2v >= ndof || idx3u >= ndof || idx3v >= ndof) {
			free(M_diag);
			return 1;
		}
		M_diag[idx1u] += me; M_diag[idx1v] += me;
		M_diag[idx2u] += me; M_diag[idx2v] += me;
		M_diag[idx3u] += me; M_diag[idx3v] += me;
	}
	*M_diag_out = M_diag;
	return 0;
}

// constraints application for modal analysis will be added when BCs are wired

int compute_modal_eigenpairs(int numModes, double **K, double *M_diag, int ndof,
		double tol, int maxIter,
		double *eigenVals, double **eigenVecs) {
	if (!K || !M_diag || !eigenVals || !eigenVecs || numModes < 1 || ndof < 1) return 1;
	// simple sequential inverse iteration on K^{-1} M with M-orthogonalization
	static int rand_initialized = 0;
	if (!rand_initialized) { srand(12345); rand_initialized = 1; }
	for (int m = 0; m < numModes; m++) {
		double *v = eigenVecs[m];
		if (!v) return 1;
		// init random
		for (int i = 0; i < ndof; i++) v[i] = (double)rand() / RAND_MAX;
		// M-orthogonalize vs previous modes
		for (int p = 0; p < m; p++) {
			double dot = 0.0;
			for (int i = 0; i < ndof; i++) dot += M_diag[i] * v[i] * eigenVecs[p][i];
			for (int i = 0; i < ndof; i++) v[i] -= dot * eigenVecs[p][i];
		}
		normalize_M(v, M_diag, ndof);
		double prevLambda = 0.0;
		for (int it = 0; it < maxIter; it++) {
			// y = K^{-1} (M v) using CG
			double *Mv = (double *)malloc(ndof * sizeof(double));
			double *y = (double *)malloc(ndof * sizeof(double));
			for (int i = 0; i < ndof; i++) Mv[i] = M_diag[i] * v[i];
			cg_solve(K, Mv, y, ndof, 1000, 1e-8);
			free(Mv);
			// M-orthogonalize y vs previous modes
			for (int p = 0; p < m; p++) {
				double dot = 0.0;
				for (int i = 0; i < ndof; i++) dot += M_diag[i] * y[i] * eigenVecs[p][i];
				for (int i = 0; i < ndof; i++) y[i] -= dot * eigenVecs[p][i];
			}
			// normalize to M-norm
			for (int i = 0; i < ndof; i++) v[i] = y[i];
			free(y);
			normalize_M(v, M_diag, ndof);
			double lambda = rayleigh_generalized(v, K, M_diag, ndof);
			if (fabs(lambda - prevLambda) < tol * fmax(1.0, lambda)) {
				eigenVals[m] = lambda;
				break;
			}
			prevLambda = lambda;
		}
	}
	return 0;
}

