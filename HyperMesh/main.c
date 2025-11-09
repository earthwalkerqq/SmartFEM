#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "meshGenerate.h"

int main(int argc, char **argv) {
    const char *cadFile = (argc > 1) ? argv[1] : "../../materials/data-sample/52.stp";
    
    // Проверяем, передан ли параметр как количество элементов или как clmin/clmax
    if (argc > 2) {
        // Пробуем распарсить второй аргумент как число элементов
        char *endptr;
        long targetElements = strtol(argv[2], &endptr, 10);
        
        // Если это число и третий аргумент не задан (или это тоже число), используем режим с заданным количеством элементов
        if (*endptr == '\0' && targetElements > 0 && (argc == 3 || (argc == 4 && strtol(argv[3], &endptr, 10) > 0))) {
            // Режим генерации с заданным количеством элементов
            int target = (int)targetElements;
            double finalClmin = 0.0, finalClmax = 0.0;
            int actualElements = 0;
            
            printf("Generating mesh with target elements: %d\n", target);
            fflush(stdout);
            
            bool_t result = hyperMeshWithTargetElements((char *)cadFile, target, &finalClmin, &finalClmax, &actualElements);
            
            if (result == EXIT_SUCCESS) {
                printf("Mesh generated successfully!\n");
                printf("Target elements: %d\n", target);
                printf("Actual elements: %d\n", actualElements);
                printf("Final parameters: clmin=%.4f, clmax=%.4f\n", finalClmin, finalClmax);
                printf("Output: ../build/node.txt\n");
                return 0;
            } else {
                fprintf(stderr, "Failed to generate mesh\n");
                return 1;
            }
        }
    }
    
    // Старый режим: clmin и clmax
    double clmin = (argc > 2) ? atof(argv[2]) : 0.1;
    double clmax = (argc > 3) ? atof(argv[3]) : 1.0;

    printf("Generating mesh from: %s\n", cadFile);
    fflush(stdout);
    printf("Mesh size: min=%.2f, max=%.2f\n", clmin, clmax);
    fflush(stdout);

    bool_t result = hyperMesh((char *)cadFile, clmin, clmax);

    if (result == EXIT_SUCCESS) {
        printf("Mesh generated successfully!\n");
        printf("Output: ../build/node.txt\n");
        return 0;
    } else {
        fprintf(stderr, "Failed to generate mesh\n");
        return 1;
    }
}

