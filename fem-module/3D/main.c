#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>  // для getcwd, getenv
#include <string.h>  // для strncpy, snprintf
#include "modal.h"
#include "../2D/inc/bc.h"  // для применения граничных условий

// ANSI цветовые коды для терминала
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"

// Макросы для цветного вывода
#define PRINT_SUCCESS(fmt, ...) printf(COLOR_GREEN fmt COLOR_RESET, ##__VA_ARGS__)
#define PRINT_ERROR(fmt, ...) fprintf(stderr, COLOR_RED fmt COLOR_RESET, ##__VA_ARGS__)
#define PRINT_WARNING(fmt, ...) fprintf(stderr, COLOR_YELLOW fmt COLOR_RESET, ##__VA_ARGS__)

int main(int argc, char **argv) {
	const char *meshFile = (argc > 1) ? argv[1] : "../../materials/data-sample/node.txt";
	int targetModes = (argc > 2) ? atoi(argv[2]) : 3;
	double rho = (argc > 3) ? atof(argv[3]) : 7850.0; // steel density kg/m^3
	double h = (argc > 4) ? atof(argv[4]) : 1.0;     // thickness

	PRINT_SUCCESS("Reading mesh from: %s\n", meshFile);
	fflush(stdout);
	
	double **K = NULL; double *K_data = NULL; int ndof = 0, nNodes = 0, nElem = 0; double **car = NULL; int **jt03 = NULL;
	if (assemble_global_stiffness_from_mesh(meshFile, &K, &K_data, &ndof, &nNodes, &nElem, &car, &jt03)) {
		PRINT_ERROR("Failed to read mesh or assemble K from %s\n", meshFile);
		return 1;
	}
	PRINT_SUCCESS("Assembled K: nNodes=%d, nElem=%d, ndof=%d\n", nNodes, nElem, ndof);
	fflush(stdout);
	if (K == NULL || K_data == NULL || car == NULL || jt03 == NULL) {
		PRINT_ERROR("NULL pointer after assembly\n");
		return 1;
	}

	PRINT_SUCCESS("Building mass matrix...\n");
	fflush(stdout);
	
	double *M_diag = NULL;
	if (build_lumped_mass(&M_diag, ndof, nElem, jt03, car, rho, h)) {
		PRINT_ERROR("Failed to build mass matrix\n");
		return 1;
	}
	PRINT_SUCCESS("Mass matrix built successfully.\n");
	fflush(stdout);
	
	// Применяем граничные условия к матрице жесткости
	PRINT_SUCCESS("Applying boundary conditions...\n");
	fflush(stdout);
	
	int lenNodePres = 0, lenNodeZakrU = 0, lenNodeZakrV = 0;
	int *nodePres = NULL;
	int *nodeZakrU = NULL;
	int *nodeZakrV = NULL;
	
	// Находим закрепленные узлы (индексы инициализированы в 0)
	FillConstrainedLoadedNodes(&nodePres, &lenNodePres, &nodeZakrU, &lenNodeZakrU, &nodeZakrV, &lenNodeZakrV, car, nNodes);
	
	// Проверка наличия достаточного количества закреплений
	// Для модального анализа нужно хотя бы одно закрепление в каждом направлении
	// ИЛИ хотя бы одно закрепление по обеим осям (узлы с u_flag=0 и v_flag=0)
	int totalConstrained = lenNodeZakrU + lenNodeZakrV;
	
	// Проверяем, есть ли узлы, закрепленные по обеим осям
	int bothConstrained = 0;
	for (int i = 0; i < nNodes; i++) {
		if ((int)car[3][i] == 0 && (int)car[4][i] == 0) {
			bothConstrained++;
		}
	}
	
	// Для модального анализа требуется хотя бы одно закрепление
	// Если есть узлы, закрепленные по обеим осям, это достаточно
	// Или если есть закрепления и по U, и по V (даже на разных узлах)
	if (totalConstrained == 0 && bothConstrained == 0) {
		PRINT_ERROR("Ошибка: недостаточно граничных условий. U_fixed=%d, V_fixed=%d, Both_fixed=%d\n", 
		            lenNodeZakrU, lenNodeZakrV, bothConstrained);
		if (nodePres) free(nodePres);
		if (nodeZakrU) free(nodeZakrU);
		if (nodeZakrV) free(nodeZakrV);
		free(M_diag);
		free(K);
		free(K_data);
		return 1;
	}
	
	// Если есть только закрепления по одной оси, это может быть проблемой для модального анализа
	// Но мы все равно попробуем выполнить расчет
	if (lenNodeZakrU == 0 && lenNodeZakrV > 0) {
		PRINT_WARNING("Предупреждение: закрепления только по V (Y). U_fixed=%d, V_fixed=%d\n", lenNodeZakrU, lenNodeZakrV);
	} else if (lenNodeZakrU > 0 && lenNodeZakrV == 0) {
		PRINT_WARNING("Предупреждение: закрепления только по U (X). U_fixed=%d, V_fixed=%d\n", lenNodeZakrU, lenNodeZakrV);
	} else {
		PRINT_SUCCESS("Boundary conditions: U_fixed=%d, V_fixed=%d, Both_fixed=%d\n", 
		              lenNodeZakrU, lenNodeZakrV, bothConstrained);
	}
	fflush(stdout);
	
	const int ndofysla = 2;
	
	// Сначала собираем все закрепленные DOF в один массив для избежания дублирования
	int *constrainedDOFs = (int *)malloc((lenNodeZakrU + lenNodeZakrV) * sizeof(int));
	int numConstrained = 0;
	
	// Для узлов, закрепленных по V (v_flag==0): закрепляем только V компоненту (DOF = node*2 + 1)
	for (int i = 0; i < lenNodeZakrV; i++) {
		int node = nodeZakrV[i];
		int kdof = node * ndofysla + 1;  // V компонента (индекс 1)
		if (kdof >= 0 && kdof < ndof) {
			constrainedDOFs[numConstrained++] = kdof;
		}
	}
	
	// Для узлов, закрепленных по U (u_flag==0): закрепляем только U компоненту (DOF = node*2 + 0)
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
	
	PRINT_SUCCESS("Boundary conditions applied successfully.\n");
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
				PRINT_WARNING("Warning: Found negative v^T K v = %.6e (matrix may not be positive definite)\n", vKv);
				fflush(stdout);
			}
		}
		free(testVec);
	}
	if (checkCount > 0) {
		PRINT_WARNING("Warning: %d out of 10 test vectors gave negative v^T K v (min = %.6e)\n", checkCount, minVKv);
		fflush(stdout);
	} else {
		PRINT_SUCCESS("Matrix K appears to be positive definite (min v^T K v = %.6e)\n", minVKv);
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

	PRINT_SUCCESS("Computing %d eigenpairs...\n", targetModes);
	PRINT_SUCCESS("This may take some time for large meshes...\n");
	fflush(stdout);
	
	compute_modal_eigenpairs(targetModes, K, M_diag, ndof, 1e-8, 500, eigs, modes);

	PRINT_SUCCESS("Computed %d modes (ndof=%d)\n", targetModes, ndof);
	fflush(stdout);
	
	// Записываем результаты в файл Modal_result.txt на рабочий стол
	// Определяем путь к рабочему столу
	const char *home = getenv("HOME");
	char desktopPath[2048];
	if (home != NULL) {
		snprintf(desktopPath, sizeof(desktopPath), "%s/Desktop/Modal_result.txt", home);
	} else {
		// Если HOME не установлен, используем текущую директорию
		strncpy(desktopPath, "Modal_result.txt", sizeof(desktopPath) - 1);
		desktopPath[sizeof(desktopPath) - 1] = '\0';
	}
	
	PRINT_SUCCESS("Saving results to: %s\n", desktopPath);
	fflush(stdout);
	
	FILE *fp = fopen(desktopPath, "w");
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
			PRINT_SUCCESS("mode %d: lambda=%.6e, omega=%.6e rad/s, f=%.6e Hz\n", i + 1, omega2, omega, freq);
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
		PRINT_SUCCESS("Results saved to: %s\n", desktopPath);
	} else {
		PRINT_ERROR("Warning: Could not open result file: %s\n", desktopPath);
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
