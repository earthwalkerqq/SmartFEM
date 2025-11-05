#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "modal.h"

int main(int argc, char **argv) {
	const char *meshFile = (argc > 1) ? argv[1] : "../../materials/data-sample/node.txt";
	int targetModes = (argc > 2) ? atoi(argv[2]) : 3;
	double rho = (argc > 3) ? atof(argv[3]) : 7850.0; // steel density kg/m^3
	double h = (argc > 4) ? atof(argv[4]) : 1.0;     // thickness

	printf("Reading mesh from: %s\n", meshFile);
	double **K = NULL; double *K_data = NULL; int ndof = 0, nNodes = 0, nElem = 0; double **car = NULL; int **jt03 = NULL;
	if (assemble_global_stiffness_from_mesh(meshFile, &K, &K_data, &ndof, &nNodes, &nElem, &car, &jt03)) {
		fprintf(stderr, "Failed to read mesh or assemble K from %s\n", meshFile);
		return 1;
	}
	printf("Assembled K: nNodes=%d, nElem=%d, ndof=%d\n", nNodes, nElem, ndof);
	if (K == NULL || K_data == NULL || car == NULL || jt03 == NULL) {
		fprintf(stderr, "NULL pointer after assembly\n");
		return 1;
	}

	double *M_diag = NULL;
	if (build_lumped_mass(&M_diag, ndof, nElem, jt03, car, rho, h)) {
		fprintf(stderr, "Failed to build mass matrix\n");
		return 1;
	}

	if (targetModes < 1) targetModes = 1;
	double *eigs = (double *)malloc(targetModes * sizeof(double));
	double **modes = (double **)malloc(targetModes * sizeof(double *));
	for (int i = 0; i < targetModes; i++) modes[i] = (double *)malloc(ndof * sizeof(double));

	compute_modal_eigenpairs(targetModes, K, M_diag, ndof, 1e-8, 500, eigs, modes);

	printf("Computed %d modes (ndof=%d)\n", targetModes, ndof);
	for (int i = 0; i < targetModes; i++) {
		double omega2 = eigs[i];
		double omega = (omega2 > 0.0) ? sqrt(omega2) : 0.0;
		double freq = omega / (2.0 * 3.141592653589793);
		printf("mode %d: lambda=%.6e, omega=%.6e rad/s, f=%.6e Hz\n", i + 1, omega2, omega, freq);
	}

	// cleanup (partial; car/jt03 are owned by IO allocs)
	for (int i = 0; i < targetModes; i++) free(modes[i]);
	free(modes);
	free(eigs);
	free(M_diag);
	free(K);
	free(K_data);

	return 0;
}

