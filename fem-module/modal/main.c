#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>  // для getcwd
#include "modal.h"
#include "../2D/inc/bc.h"  // для применения граничных условий

int main(int argc, char **argv) {
	const char *meshFile = (argc > 1) ? argv[1] : "../../materials/data-sample/node.txt";
	int targetModes = (argc > 2) ? atoi(argv[2]) : 3;
	double rho = (argc > 3) ? atof(argv[3]) : 7850.0; // steel density kg/m^3
	double h = (argc > 4) ? atof(argv[4]) : 1.0;     // thickness
	
	// Определяем путь к файлу результатов относительно исполняемого файла
	// Исполняемый файл находится в fem-module/build/modal
	// Результаты сохраняем в fem-module/build/Modal_result.txt
	const char *resultFile = "Modal_result.txt";  // Относительно рабочей директории (fem-module/build)

	printf("Reading mesh from: %s\n", meshFile);
	fflush(stdout);  // Принудительно выводим в консоль
	
	double **K = NULL; double *K_data = NULL; int ndof = 0, nNodes = 0, nElem = 0; double **car = NULL; int **jt03 = NULL;
	if (assemble_global_stiffness_from_mesh(meshFile, &K, &K_data, &ndof, &nNodes, &nElem, &car, &jt03)) {
		fprintf(stderr, "Failed to read mesh or assemble K from %s\n", meshFile);
		return 1;
	}
	printf("Assembled K: nNodes=%d, nElem=%d, ndof=%d\n", nNodes, nElem, ndof);
	fflush(stdout);
	if (K == NULL || K_data == NULL || car == NULL || jt03 == NULL) {
		fprintf(stderr, "NULL pointer after assembly\n");
		return 1;
	}

	printf("Building mass matrix...\n");
	fflush(stdout);
	
	double *M_diag = NULL;
	if (build_lumped_mass(&M_diag, ndof, nElem, jt03, car, rho, h)) {
		fprintf(stderr, "Failed to build mass matrix\n");
		return 1;
	}
	printf("Mass matrix built successfully.\n");
	fflush(stdout);
	
	// Применяем граничные условия к матрице жесткости
	printf("Applying boundary conditions...\n");
	fflush(stdout);
	
	int lenNodePres = 0, lenNodeZakrU = 0, lenNodeZakrV = 0;
	int *nodePres = NULL;
	int *nodeZakrU = NULL;
	int *nodeZakrV = NULL;
	
	// Находим закрепленные узлы
	FillConstrainedLoadedNodes(&nodePres, &lenNodePres, &nodeZakrU, &lenNodeZakrU, &nodeZakrV, &lenNodeZakrV, car, nNodes);
	
	// Проверка наличия достаточного количества закреплений
	if (lenNodeZakrU == 0 || lenNodeZakrV == 0) {
		fprintf(stderr, "Ошибка: недостаточно граничных условий. U_fixed=%d, V_fixed=%d\n", lenNodeZakrU, lenNodeZakrV);
		free(M_diag);
		free(K);
		free(K_data);
		return 1;
	}
	
	printf("Boundary conditions: U_fixed=%d, V_fixed=%d\n", lenNodeZakrU, lenNodeZakrV);
	fflush(stdout);
	
	const int ndofysla = 2;
	
	// Сначала собираем все закрепленные DOF в один массив для избежания дублирования
	int *constrainedDOFs = (int *)malloc((lenNodeZakrU + lenNodeZakrV) * sizeof(int));
	int numConstrained = 0;
	
	// Для узлов, закрепленных по V (y==0): закрепляем только V компоненту (DOF = node*2 + 1)
	for (int i = 0; i < lenNodeZakrV; i++) {
		int node = nodeZakrV[i];
		int kdof = node * ndofysla + 1;  // V компонента (индекс 1)
		if (kdof >= 0 && kdof < ndof) {
			constrainedDOFs[numConstrained++] = kdof;
		}
	}
	
	// Для узлов, закрепленных по U (x==0): закрепляем только U компоненту (DOF = node*2 + 0)
	for (int i = 0; i < lenNodeZakrU; i++) {
		int node = nodeZakrU[i];
		int kdof = node * ndofysla + 0;  // U компонента (индекс 0)
		if (kdof >= 0 && kdof < ndof) {
			constrainedDOFs[numConstrained++] = kdof;
		}
	}
	
	// Применяем закрепления ко всем закрепленным DOF
	for (int c = 0; c < numConstrained; c++) {
		int kdof = constrainedDOFs[c];
		// Обнуляем строку и столбец, кроме диагонали, и устанавливаем большое значение на диагонали
		for (int j = 0; j < ndof; j++) {
			if (j != kdof) {
				K[kdof][j] = 0.0;
				K[j][kdof] = 0.0;
			}
		}
		K[kdof][kdof] = 1.e38;
		// Обнуляем массу для закрепленного DOF (чтобы он не участвовал в колебаниях)
		M_diag[kdof] = 1.0e-10;  // Очень маленькая масса для закрепленного DOF
	}
	
	free(constrainedDOFs);
	
	printf("Boundary conditions applied successfully.\n");
	fflush(stdout);
	
	// Проверяем, что матрица K положительно определенная после применения граничных условий
	// Проверяем несколько случайных векторов
	int checkCount = 0;
	double minVKv = 1.e30;
	srand(12345);  // Инициализируем генератор случайных чисел
	for (int test = 0; test < 10; test++) {
		double *testVec = (double *)malloc(ndof * sizeof(double));
		for (int i = 0; i < ndof; i++) {
			if (K[i][i] > 1.e30) {
				testVec[i] = 0.0;  // Обнуляем закрепленные DOF
			} else {
				testVec[i] = ((double)rand() / RAND_MAX) - 0.5;  // Случайный вектор
			}
		}
		// Вычисляем v^T K v
		double vKv = 0.0;
		for (int i = 0; i < ndof; i++) {
			if (K[i][i] > 1.e30) continue;
			double Kv_i = 0.0;
			for (int j = 0; j < ndof; j++) {
				if (K[j][j] <= 1.e30) {
					Kv_i += K[i][j] * testVec[j];
				}
			}
			vKv += testVec[i] * Kv_i;
		}
		if (vKv < minVKv) minVKv = vKv;
		if (vKv < 0.0) {
			checkCount++;
			if (checkCount == 1) {
				printf("Warning: Found negative v^T K v = %.6e (matrix may not be positive definite)\n", vKv);
				fflush(stdout);
			}
		}
		free(testVec);
	}
	if (checkCount > 0) {
		printf("Warning: %d out of 10 test vectors gave negative v^T K v (min = %.6e)\n", checkCount, minVKv);
		fflush(stdout);
	} else {
		printf("Matrix K appears to be positive definite (min v^T K v = %.6e)\n", minVKv);
		fflush(stdout);
	}
	
	// Освобождаем память для массивов узлов
	if (nodePres) free(nodePres);
	if (nodeZakrU) free(nodeZakrU);
	if (nodeZakrV) free(nodeZakrV);

	if (targetModes < 1) targetModes = 1;
	double *eigs = (double *)malloc(targetModes * sizeof(double));
	double **modes = (double **)malloc(targetModes * sizeof(double *));
	for (int i = 0; i < targetModes; i++) modes[i] = (double *)malloc(ndof * sizeof(double));

	printf("Computing %d eigenpairs...\n", targetModes);
	printf("This may take some time for large meshes...\n");
	fflush(stdout);
	
	compute_modal_eigenpairs(targetModes, K, M_diag, ndof, 1e-8, 500, eigs, modes);

	printf("Computed %d modes (ndof=%d)\n", targetModes, ndof);
	fflush(stdout);
	
	// Записываем результаты в файл Modal_result.txt
	// Файл будет создан в рабочей директории (fem-module/build)
	// Выводим полный путь для отладки
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("Current working directory: %s\n", cwd);
		printf("Result file path: %s/%s\n", cwd, resultFile);
	}
	
	FILE *fp = fopen(resultFile, "w");
	if (fp) {
		fprintf(fp, "=== Модальный анализ ===\n");
		fprintf(fp, "Количество мод: %d\n", targetModes);
		fprintf(fp, "Количество степеней свободы: %d\n", ndof);
		fprintf(fp, "Параметры: rho=%.2f kg/m^3, h=%.2f m\n\n", rho, h);
		fprintf(fp, "Результаты:\n");
		fprintf(fp, "%-6s %-15s %-15s %-15s\n", "Мода", "Lambda (ω²)", "Omega (rad/s)", "Частота (Hz)");
		fprintf(fp, "------------------------------------------------------------\n");
		
		for (int i = 0; i < targetModes; i++) {
			double omega2 = eigs[i];
			double omega = (omega2 > 0.0) ? sqrt(omega2) : 0.0;
			double freq = omega / (2.0 * 3.141592653589793);
			printf("mode %d: lambda=%.6e, omega=%.6e rad/s, f=%.6e Hz\n", i + 1, omega2, omega, freq);
			fprintf(fp, "%-6d %-15.6e %-15.6e %-15.6e\n", i + 1, omega2, omega, freq);
		}
		
		fprintf(fp, "\n=== Формы колебаний ===\n");
		fprintf(fp, "Смещения узлов для каждой моды:\n");
		for (int i = 0; i < targetModes; i++) {
			double omega2 = eigs[i];
			double omega = (omega2 > 0.0) ? sqrt(omega2) : 0.0;
			double freq = omega / (2.0 * 3.141592653589793);
			fprintf(fp, "\nМода %d (f=%.6e Hz):\n", i + 1, freq);
			fprintf(fp, "%-8s %-15s %-15s\n", "Узел", "U (смещение X)", "V (смещение Y)");
			fprintf(fp, "------------------------------------------------------------\n");
			
			// Выводим смещения узлов (ndof = 2 * nNodes для 2D)
			int nNodes = ndof / 2;
			for (int node = 0; node < nNodes && node < 100; node++) {  // Ограничиваем вывод первыми 100 узлами
				int uIdx = node * 2;
				int vIdx = node * 2 + 1;
				if (uIdx < ndof && vIdx < ndof) {
					fprintf(fp, "%-8d %-15.6e %-15.6e\n", node + 1, modes[i][uIdx], modes[i][vIdx]);
				}
			}
			if (nNodes > 100) {
				fprintf(fp, "... (показаны первые 100 узлов из %d)\n", nNodes);
			}
		}
		
		fclose(fp);
		printf("Results saved to: %s\n", resultFile);
	} else {
		fprintf(stderr, "Warning: Could not open result file: %s\n", resultFile);
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

