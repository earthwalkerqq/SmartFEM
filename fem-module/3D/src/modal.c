#include <stdio.h>
#include "modal.h"
// reuse 2D FEM IO and assembly
#include "io.h"
#include "fem.h"
#include "bc.h"  // для применения граничных условий
#include <math.h>
#include <stdlib.h>

void print_modal_info(const modal_info *info) {
	printf("modes=%d, ndof=%d\n", info ? info->numModes : 0, info ? info->ndof : 0);
}

static void normalize_M(double *v, const double *M_diag, int ndof, double **K) {
	double dot = 0.0;
	for (int i = 0; i < ndof; i++) {
		// Пропускаем закрепленные DOF
		if (K[i][i] <= 1.e30) {
			dot += M_diag[i] * v[i] * v[i];
		}
	}
	double invNorm = (dot > 0.0) ? 1.0 / sqrt(dot) : 1.0;
	for (int i = 0; i < ndof; i++) {
		if (K[i][i] <= 1.e30) {
			v[i] *= invNorm;
		} else {
			v[i] = 0.0;  // Убеждаемся, что закрепленные DOF обнулены
		}
	}
}

static double rayleigh_generalized(const double *v, double **K, const double *M_diag, int ndof) {
	double num = 0.0, den = 0.0;
	
	double vNormSq = 0.0;
	
	// Подсчитываем норму вектора
	for (int i = 0; i < ndof; i++) {
		if (K[i][i] <= 1.e30) {
			vNormSq += v[i] * v[i];
		}
	}
	
	// Если вектор слишком маленький, возвращаем 0
	if (vNormSq < 1.e-30) {
		return 0.0;
	}
	
	for (int i = 0; i < ndof; i++) {
		// Пропускаем закрепленные DOF
		if (K[i][i] > 1.e30) continue;
		
		// Вычисляем (K*v)[i] только для незакрепленных DOF
		double Kv_i = 0.0;
		for (int j = 0; j < ndof; j++) {
			// Пропускаем закрепленные DOF в сумме
			if (K[j][j] > 1.e30) continue;
			Kv_i += K[i][j] * v[j];
		}
		
		num += v[i] * Kv_i;
		
		// Добавляем в знаменатель только если масса достаточно большая
		if (M_diag[i] > 1.e-12) {
			den += M_diag[i] * v[i] * v[i];
		}
	}
	
	// Проверяем, что знаменатель не слишком мал
	if (den <= 1.e-30) {
		// Попробуем пересчитать без фильтрации маленьких компонент
		den = 0.0;
		num = 0.0;
		for (int i = 0; i < ndof; i++) {
			if (K[i][i] > 1.e30) continue;
			
			double Kv_i = 0.0;
			for (int j = 0; j < ndof; j++) {
				if (K[j][j] > 1.e30) continue;
				Kv_i += K[i][j] * v[j];
			}
			num += v[i] * Kv_i;
			den += M_diag[i] * v[i] * v[i];
		}
	}
	
	if (den <= 1.e-30) {
		return 0.0;  // Знаменатель все еще слишком мал
	}
	
	double lambda = num / den;
	
	// Проверяем, что lambda положительное (для модального анализа это должно быть так)
	// Если lambda отрицательное, это означает, что матрица K не положительно определенная
	// или вектор v находится в неправильном подпространстве
	if (lambda < 0.0) {
		// Если lambda отрицательное, это серьезная проблема
		// Возможно, матрица K не положительно определенная или вектор v некорректный
		// Возвращаем 0, чтобы алгоритм мог обработать это
		return 0.0;
	}
	
	return lambda;
}

static void K_mul(const double **K, const double *x, double *y, int n) {
	for (int i = 0; i < n; i++) {
		double s = 0.0;
		// Для закрепленных DOF результат всегда 0 (кроме диагонали, которая очень большая)
		if (K[i][i] > 1.e30) {
			// Для закрепленного DOF: y[i] = K[i][i] * x[i], но x[i] должно быть 0
			y[i] = K[i][i] * x[i];  // Это будет 0, так как x[i] = 0 для закрепленных DOF
		} else {
			for (int j = 0; j < n; j++) {
				// Пропускаем закрепленные DOF в сумме
				if (K[j][j] <= 1.e30) {
					s += K[i][j] * x[j];
				}
			}
			y[i] = s;
		}
	}
}

static void cg_solve(double **K, const double *b, double *x, int n, int maxIter, double tol) {
	for (int i = 0; i < n; i++) x[i] = 0.0;
	double *r = (double *)malloc(n * sizeof(double));
	double *p = (double *)malloc(n * sizeof(double));
	double *Ap = (double *)malloc(n * sizeof(double));
	// r = b - Kx (x=0 initially)
	for (int i = 0; i < n; i++) r[i] = b[i];
	
	// Для закрепленных DOF (где K[i][i] очень большое), сразу устанавливаем x[i] = 0
	// и обнуляем соответствующую компоненту невязки
	for (int i = 0; i < n; i++) {
		if (K[i][i] > 1.e30) {  // Закрепленный DOF
			x[i] = 0.0;
			r[i] = 0.0;
		}
	}
	
	for (int i = 0; i < n; i++) p[i] = r[i];
	double rsold = 0.0; for (int i = 0; i < n; i++) rsold += r[i] * r[i];
	for (int it = 0; it < maxIter; it++) {
		K_mul((const double **)K, p, Ap, n);
		
		// Для закрепленных DOF обнуляем Ap и p
		for (int i = 0; i < n; i++) {
			if (K[i][i] > 1.e30) {  // Закрепленный DOF
				Ap[i] = 0.0;
				p[i] = 0.0;
			}
		}
		
		double pAp = 0.0; for (int i = 0; i < n; i++) pAp += p[i] * Ap[i];
		if (fabs(pAp) < 1e-20) break;
		double alpha = rsold / pAp;
		for (int i = 0; i < n; i++) {
			if (K[i][i] <= 1.e30) {  // Только для незакрепленных DOF
				x[i] += alpha * p[i];
				r[i] -= alpha * Ap[i];
			}
		}
		double rsnew = 0.0; for (int i = 0; i < n; i++) rsnew += r[i] * r[i];
		if (sqrt(rsnew) < tol) break;
		double beta = rsnew / rsold;
		for (int i = 0; i < n; i++) {
			if (K[i][i] <= 1.e30) {  // Только для незакрепленных DOF
				p[i] = r[i] + beta * p[i];
			} else {
				p[i] = 0.0;
			}
		}
		rsold = rsnew;
	}
	
	// Финальная проверка: принудительно обнуляем закрепленные DOF в решении
	for (int i = 0; i < n; i++) {
		if (K[i][i] > 1.e30) {
			x[i] = 0.0;  // Принудительно обнуляем закрепленные DOF
		}
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
		printf("Computing mode %d/%d...\n", m + 1, numModes);
		fflush(stdout);
		
		double *v = eigenVecs[m];
		if (!v) return 1;
		
		// Инициализируем случайный вектор только для свободных DOF
		// Используем более широкий диапазон значений для лучшей инициализации
		int freeDOFCount = 0;
		for (int i = 0; i < ndof; i++) {
			if (K[i][i] > 1.e30) {
				v[i] = 0.0;  // Закрепленный DOF
			} else {
				// Генерируем случайное значение от -1 до 1
				v[i] = 2.0 * ((double)rand() / RAND_MAX) - 1.0;
				freeDOFCount++;
			}
		}
		
		if (freeDOFCount == 0) {
			printf("  Error: Mode %d: no free DOF available\n", m + 1);
			fflush(stdout);
			eigenVals[m] = 0.0;
			continue;
		}
		
		// M-orthogonalize vs previous modes
		for (int p = 0; p < m; p++) {
			double dot = 0.0;
			for (int i = 0; i < ndof; i++) {
				// Пропускаем закрепленные DOF
				if (K[i][i] <= 1.e30) {
					dot += M_diag[i] * v[i] * eigenVecs[p][i];
				}
			}
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] <= 1.e30) {
					v[i] -= dot * eigenVecs[p][i];
				} else {
					v[i] = 0.0;
				}
			}
		}
		
		// Проверяем норму вектора перед нормализацией
		double vNormBefore = 0.0;
		for (int i = 0; i < ndof; i++) {
			if (K[i][i] <= 1.e30) {
				vNormBefore += v[i] * v[i];
			}
		}
		
		if (vNormBefore < 1.e-20) {
			printf("  Warning: Mode %d: vector is zero before normalization, reinitializing...\n", m + 1);
			fflush(stdout);
			// Переинициализируем вектор
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] > 1.e30) {
					v[i] = 0.0;
				} else {
					v[i] = 2.0 * ((double)rand() / RAND_MAX) - 1.0;
				}
			}
			// Повторяем ортогонализацию
			for (int p = 0; p < m; p++) {
				double dot = 0.0;
				for (int i = 0; i < ndof; i++) {
					if (K[i][i] <= 1.e30) {
						dot += M_diag[i] * v[i] * eigenVecs[p][i];
					}
				}
				for (int i = 0; i < ndof; i++) {
					if (K[i][i] <= 1.e30) {
						v[i] -= dot * eigenVecs[p][i];
					} else {
						v[i] = 0.0;
					}
				}
			}
		}
		
		normalize_M(v, M_diag, ndof, K);
		
		// Проверяем, что вектор не нулевой после нормализации
		double vNormCheck = 0.0;
		double MNormCheck = 0.0;
		for (int i = 0; i < ndof; i++) {
			if (K[i][i] <= 1.e30) {
				vNormCheck += v[i] * v[i];
				MNormCheck += M_diag[i] * v[i] * v[i];
			}
		}
		
		if (vNormCheck < 1.e-20 || MNormCheck < 1.e-20) {
			printf("  Error: Mode %d: vector is zero after normalization (vNorm=%.6e, MNorm=%.6e, freeDOF=%d)\n", 
			       m + 1, sqrt(vNormCheck), sqrt(MNormCheck), freeDOFCount);
			fflush(stdout);
			
			// Выводим информацию о массе для диагностики
			double minMass = 1.e30;
			double maxMass = 0.0;
			int nonzeroMassCount = 0;
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] <= 1.e30) {
					if (M_diag[i] > 1.e-12) {
						nonzeroMassCount++;
						if (M_diag[i] < minMass) minMass = M_diag[i];
						if (M_diag[i] > maxMass) maxMass = M_diag[i];
					}
				}
			}
			printf("  Debug: freeDOF=%d, nonzeroMass=%d, minMass=%.6e, maxMass=%.6e\n", 
			       freeDOFCount, nonzeroMassCount, minMass, maxMass);
			fflush(stdout);
			
			eigenVals[m] = 0.0;
			continue;
		}
		
		double prevLambda = 0.0;
		for (int it = 0; it < maxIter; it++) {
			if (it > 0 && it % 50 == 0) {
				printf("  Mode %d: iteration %d/%d, lambda=%.6e\n", m + 1, it, maxIter, prevLambda);
				fflush(stdout);
			}
			// y = K^{-1} (M v) using CG
			double *Mv = (double *)malloc(ndof * sizeof(double));
			double *y = (double *)malloc(ndof * sizeof(double));
			
			// Вычисляем Mv
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] <= 1.e30) {
					Mv[i] = M_diag[i] * v[i];
				} else {
					Mv[i] = 0.0;  // Обнуляем для закрепленных DOF
				}
			}
			
			// Решаем систему K y = Mv
			// Увеличиваем точность CG для более стабильного решения
			cg_solve(K, Mv, y, ndof, 2000, 1e-10);
			free(Mv);
			
			// Принудительно обнуляем закрепленные DOF в решении CG
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] > 1.e30) {
					y[i] = 0.0;  // Принудительно обнуляем закрепленные DOF
				}
			}
			
			// Проверяем, что решение y не нулевое
			double yNormSq = 0.0;
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] <= 1.e30) {
					yNormSq += y[i] * y[i];
				}
			}
			
			if (yNormSq < 1.e-30) {
				printf("  Warning: Mode %d: CG solution is zero at iteration %d\n", m + 1, it + 1);
				fflush(stdout);
				free(y);
				// Если решение нулевое, продолжаем с предыдущим вектором
				break;
			}
			
			// M-orthogonalize y vs previous modes
			for (int p = 0; p < m; p++) {
				double dot = 0.0;
				for (int i = 0; i < ndof; i++) {
					// Пропускаем закрепленные DOF
					if (K[i][i] <= 1.e30) {
						dot += M_diag[i] * y[i] * eigenVecs[p][i];
					}
				}
				for (int i = 0; i < ndof; i++) {
					if (K[i][i] <= 1.e30) {
						y[i] -= dot * eigenVecs[p][i];
					} else {
						y[i] = 0.0;
					}
				}
			}
			
			// Обновляем v = y и обнуляем закрепленные DOF
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] > 1.e30) {
					v[i] = 0.0;
				} else {
					v[i] = y[i];
				}
			}
			
			free(y);
			
			// Нормализуем вектор
			normalize_M(v, M_diag, ndof, K);
			
			// Проверяем, что вектор не стал нулевым после нормализации
			double vNormAfter = 0.0;
			for (int i = 0; i < ndof; i++) {
				if (K[i][i] <= 1.e30) {
					vNormAfter += v[i] * v[i];
				}
			}
			
			if (vNormAfter < 1.e-30) {
				printf("  Warning: Mode %d: vector became zero after normalization at iteration %d\n", m + 1, it + 1);
				fflush(stdout);
				break;
			}
			
			double lambda = rayleigh_generalized(v, K, M_diag, ndof);
			
			// Отладочный вывод для первых нескольких итераций
			if (it < 3 || (it % 10 == 0 && it < 50)) {
				// Вычисляем числитель и знаменатель для диагностики
				double num_debug = 0.0, den_debug = 0.0;
				for (int i = 0; i < ndof; i++) {
					if (K[i][i] > 1.e30) continue;
					double Kv_i = 0.0;
					for (int j = 0; j < ndof; j++) {
						if (K[j][j] > 1.e30) continue;
						Kv_i += K[i][j] * v[j];
					}
					num_debug += v[i] * Kv_i;
					if (M_diag[i] > 1.e-12) {
						den_debug += M_diag[i] * v[i] * v[i];
					}
				}
				printf("  Mode %d iter %d: lambda=%.6e, num=%.6e, den=%.6e, vNorm=%.6e\n",
				       m + 1, it + 1, lambda, num_debug, den_debug, sqrt(vNormAfter));
				fflush(stdout);
			}
			
			// Проверяем, что lambda положительное
			// Если lambda отрицательное или нулевое, это означает проблему с матрицей K
			// или с вектором v. В этом случае используем предыдущее положительное значение
			if (lambda <= 0.0) {
				if (it > 0 && prevLambda > 0.0) {
					// Используем предыдущее положительное значение lambda
					double oldLambda = lambda;
					lambda = prevLambda;
					printf("  Warning: Mode %d has negative/zero lambda=%.6e at iteration %d, using previous lambda=%.6e\n", 
					       m + 1, oldLambda, it + 1, prevLambda);
					fflush(stdout);
				} else {
					// Если это первая итерация или нет предыдущего положительного значения,
					// это серьезная проблема - матрица K не положительно определенная
					printf("  Error: Mode %d has negative/zero lambda=%.6e at iteration %d and no previous positive value\n", 
					       m + 1, lambda, it + 1);
					fflush(stdout);
					// Пытаемся переинициализировать вектор
					for (int i = 0; i < ndof; i++) {
						if (K[i][i] > 1.e30) {
							v[i] = 0.0;
						} else {
							v[i] = 2.0 * ((double)rand() / RAND_MAX) - 1.0;
						}
					}
					// M-orthogonalize vs previous modes
					for (int p = 0; p < m; p++) {
						double dot = 0.0;
						for (int i = 0; i < ndof; i++) {
							if (K[i][i] <= 1.e30) {
								dot += M_diag[i] * v[i] * eigenVecs[p][i];
							}
						}
						for (int i = 0; i < ndof; i++) {
							if (K[i][i] <= 1.e30) {
								v[i] -= dot * eigenVecs[p][i];
							} else {
								v[i] = 0.0;
							}
						}
					}
					normalize_M(v, M_diag, ndof, K);
					// Пересчитываем lambda
					lambda = rayleigh_generalized(v, K, M_diag, ndof);
					if (lambda <= 0.0) {
						printf("  Error: Mode %d still has negative/zero lambda after reinitialization, skipping mode\n", m + 1);
						fflush(stdout);
						eigenVals[m] = 0.0;
						break;
					}
				}
			}
			
			// Проверяем сходимость (только если lambda положительное)
			if (lambda > 0.0) {
				if (prevLambda > 0.0 && fabs(lambda - prevLambda) < tol * fmax(1.0, lambda)) {
					eigenVals[m] = lambda;
					double omega = sqrt(lambda);
					double freq = omega / (2.0 * 3.141592653589793);
					printf("  Mode %d converged after %d iterations: lambda=%.6e, f=%.6e Hz\n", m + 1, it + 1, lambda, freq);
					fflush(stdout);
					break;
				}
				// Обновляем prevLambda только если lambda положительное
				prevLambda = lambda;
			} else if (lambda == 0.0 && it > 10) {
				// Если lambda все еще 0 после 10 итераций, что-то не так
				printf("  Error: Mode %d: lambda is still zero after %d iterations, stopping\n", m + 1, it + 1);
				fflush(stdout);
				// Попробуем вычислить lambda еще раз с более детальной диагностикой
				int freeDOF = 0;
				double totalMass = 0.0;
				for (int i = 0; i < ndof; i++) {
					if (K[i][i] <= 1.e30) {
						freeDOF++;
						totalMass += M_diag[i];
					}
				}
				printf("  Debug: freeDOF=%d, totalMass=%.6e\n", freeDOF, totalMass);
				fflush(stdout);
				break;
			}
			// Если lambda отрицательное или нулевое, не обновляем prevLambda, чтобы использовать предыдущее положительное значение
		}
		
		// Если не сошлось, сохраняем последнее значение lambda
		if (prevLambda > 0.0) {
			eigenVals[m] = prevLambda;
		} else {
			eigenVals[m] = 0.0;
			printf("  Mode %d: failed to converge, lambda=0.0\n", m + 1);
			fflush(stdout);
		}
	}
	return 0;
}


