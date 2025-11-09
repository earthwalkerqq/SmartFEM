#include <stdio.h>
#include <stdlib.h>

#include "io.h"
#include "mtrx.h"

bool_t readFromFile(const char *filename, int *nys, double **dataCar, double ***car, int *nelem, int **data_jt03,
                    int ***jt03) {
    bool_t err = FALSE;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return TRUE;
    }
    
    // Читаем количество узлов
    if (fscanf(file, "%d", nys) != 1 || *nys <= 0) {
        fprintf(stderr, "Error: Cannot read number of nodes from %s\n", filename);
        fclose(file);
        return TRUE;
    }
    
    // Проверяем разумность значения nys
    if (*nys > 1000000) {
        fprintf(stderr, "Error: Suspiciously large number of nodes: %d\n", *nys);
        fclose(file);
        return TRUE;
    }
    
    // Расширяем массив до 6 x nys для хранения координат и граничных условий
    // car[0][i] = x, car[1][i] = y, car[2][i] = z
    // car[3][i] = u_flag, car[4][i] = v_flag, car[5][i] = load_flag
    makeDoubleMtrx(dataCar, car, 6, *nys);  // массив координат узлов и граничных условий
    
    if (*car == NULL) {
        fprintf(stderr, "Error: Cannot allocate memory for nodes array\n");
        fclose(file);
        return TRUE;
    }
    
    for (int i = 0; i < *nys; i++) {
        double x, y, z;
        double u_flag = 1.0, v_flag = 1.0, load_flag = 0.0;
        // Читаем координаты
        int items_read = fscanf(file, "%lf%lf%lf", &x, &y, &z);
        if (items_read == 3) {
            (*car)[0][i] = x;
            (*car)[1][i] = y;
            (*car)[2][i] = z;
            
            // Пытаемся прочитать граничные условия (если формат расширенный)
            int bc_read = fscanf(file, "%lf%lf%lf", &u_flag, &v_flag, &load_flag);
            if (bc_read != 3) {
                // Старый формат - только координаты, граничные условия по умолчанию
                u_flag = 1.0;
                v_flag = 1.0;
                load_flag = 0.0;
            }
        }
        
        // Сохраняем флаги граничных условий
        (*car)[3][i] = u_flag;
        (*car)[4][i] = v_flag;
        (*car)[5][i] = load_flag;
    }
    
    // Читаем количество элементов
    if (fscanf(file, "%d", nelem) != 1 || *nelem <= 0) {
        fprintf(stderr, "Error: Cannot read number of elements from %s\n", filename);
        fclose(file);
        free(*dataCar);
        free(*car);
        return TRUE;
    }
    
    makeIntegerMtrx(data_jt03, jt03, 3, *nelem);  // массив номеров узлов элемента
    if (*jt03 == NULL) {
        fprintf(stderr, "Error: Cannot allocate memory for elements array\n");
        fclose(file);
        free(*dataCar);
        free(*car);
        return TRUE;
    }
    
    for (int i = 0; i < *nelem; i++) {
        if (fscanf(file, "%d%d%d", &(*jt03)[0][i], &(*jt03)[1][i], &(*jt03)[2][i]) != 3) {
            fprintf(stderr, "Error: Cannot read element %d from %s\n", i + 1, filename);
            fclose(file);
            free(*data_jt03);
            free(*jt03);
            free(*dataCar);
            free(*car);
            return TRUE;
        }
    }

    fclose(file);
    return err;
}

bool_t writeResult(const char *filename, int **jt03, double **strain, double **stress, double *r, double *u,
                   int nelem, int nys, int ndof) {
    bool_t error = FALSE;
    FILE *file = fopen(filename, "w");
    if (!file) {
        error = TRUE;
    } else {
        fprintf(file, "Число элементов - %d\n", nelem);
        fprintf(file, "Число узлов - %d\n", nys);
        fprintf(file, "Число степеней свободы - %d\n", ndof);
        fprintf(file, "\n");
        fprintf(file, "Вектор нагрузок\n");
        int index = 1;
        for (int i = 0; i <= ndof; i += 2) {
            fprintf(file, "       ru%d       %12.4e       rv%d       %12.4e\n", index, r[i], index, r[i + 1]);
            index++;
        }
        fprintf(file, "\n");
        fprintf(file, "Результат расчета перемещений\n");
        index = 1;
        for (int i = 0; i <= ndof; i += 2) {
            fprintf(file, "       u%d       %12.4e       v%d       %12.4e\n", index, u[i], index, u[i + 1]);
            index++;
        }
        fprintf(file, "\n");
        fprintf(file, "Результат расчета деформаций, напряжений\n");
        for (int j = 0; j < nelem; j++) {
            int ielem = jt03[0][j];
            fprintf(file, "       %d       ", ielem);
            for (int i = 0; i < 4; i++) {
                fprintf(file, "       %12.4f", strain[i][ielem - 1]);
            }
            fprintf(file, "       |");
            for (int i = 0; i < 4; i++) {
                fprintf(file, "       %12.4f", stress[i][ielem - 1]);
            }
            fprintf(file, "\n");
        }
        fclose(file);
    }
    return error;
}