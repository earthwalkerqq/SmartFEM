#include <gmshc.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "meshGenerate.h"

void initHyperMesh(char *inputFile, char *outputMeshFile, double clmin, double clmax, int *error) {
    *error = 0;
    fprintf(stderr, "Step 1: Initializing Gmsh...\n");
    fflush(stderr);
    gmshInitialize(1, NULL, 1, 0, error);
    if (*error) {
        fprintf(stderr, "Error initializing Gmsh: %d\n", *error);
        return;
    }
    fprintf(stderr, "Gmsh initialized successfully\n");
    fflush(stderr);
    fprintf(stderr, "Step 2: Merging file %s...\n", inputFile);
    fflush(stderr);
    gmshMerge(inputFile, error);
    if (*error) {
        fprintf(stderr, "Error merging file %s: %d\n", inputFile, *error);
        return;
    }
    fprintf(stderr, "Step 3: Synchronizing model...\n");
    fflush(stderr);
    // Синхронизация CAD-модели в ядро Gmsh
    gmshModelOccSynchronize(error);
    if (*error) {
        fprintf(stderr, "Error synchronizing model: %d\n", *error);
        return;
    }
    fprintf(stderr, "Step 4: Setting mesh options...\n");
    fflush(stderr);
    gmshOptionSetNumber("Mesh.CharacteristicLengthMax", clmax, error);
    gmshOptionSetNumber("Mesh.CharacteristicLengthMin", clmin, error);

    gmshModelMeshSetOrder(MESH_ORDER, error);
    fprintf(stderr, "Step 5: Generating mesh...\n");
    fflush(stderr);
    gmshModelMeshGenerate(DIMENSION, error);  // dim - заданное пространство
    if (*error) {
        fprintf(stderr, "Error generating mesh: %d\n", *error);
        return;
    }
    fprintf(stderr, "Step 6: Writing mesh file...\n");
    fflush(stderr);
    gmshWrite(outputMeshFile, error);
    fprintf(stderr, "Done.\n");
    fflush(stderr);
}

bool_t hyperMesh(char *cadFile, double clmin, double clmax) {
    int error = 0;

    const char *outputNodeFile = "../build/node.txt";
    const char *outputMeshFile = "../build/HyperMesh.msh";

    if (cadFile != NULL) {
        initHyperMesh((char *)cadFile, (char *)outputMeshFile, clmin, clmax, &error);

        if (error) {
            fprintf(stderr, "Error initializing mesh: %d\n", error);
            return EXIT_FAILURE;
        }

        size_t nodeTags_n, coord_n, param_n;
        double *nodeCoords, *nodeParams;
        size_t *nodeTags;

        gmshModelMeshGetNodes(&nodeTags, &nodeTags_n, &nodeCoords, &coord_n, &nodeParams, &param_n, -1, 0, 0,
                              0, &error);  // dim = -1 (все элементы)
        
        if (error) {
            fprintf(stderr, "Error getting nodes: error=%d\n", error);
            gmshFinalize(&error);
            return EXIT_FAILURE;
        }
        
        if (!nodeTags || !nodeCoords || nodeTags_n == 0) {
            fprintf(stderr, "Invalid nodes data: nodeTags_n=%zu, nodeTags=%p, nodeCoords=%p\n", 
                    nodeTags_n, (void*)nodeTags, (void*)nodeCoords);
            gmshFinalize(&error);
            return EXIT_FAILURE;
        }

        size_t maxTag = *nodeTags;
        for (size_t i = 0; i < nodeTags_n; i++) {
            if (nodeTags[i] > maxTag) maxTag = nodeTags[i];
        }

        size_t *tagIndex = (size_t *)calloc(maxTag + 1, sizeof(size_t));
        for (size_t i = 0; i < nodeTags_n; i++) {
            tagIndex[nodeTags[i]] = i + 1;
        }

        FILE *file = fopen(outputNodeFile, "w");

        assert(file != NULL);

        fprintf(file, "%zu\n", nodeTags_n);

        for (size_t i = 0; i < nodeTags_n; i++) {
            for (int j = 0; j < 3; j++) {
                (j < 2) ? fprintf(file, "%lf ", nodeCoords[i * 3 + j])
                        : fprintf(file, "%lf", nodeCoords[i * 3 + j]);
            }
            fputc('\n', file);
        }

        int *elementTypes;
        size_t elementTypes_n, elementTags_nn, elementNodes_nn;
        size_t **elementTags, **elementNodes;
        size_t *elementTags_n, *elementNodes_n;

        gmshModelMeshGetElements(&elementTypes, &elementTypes_n, &elementTags, &elementTags_n,
                                 &elementTags_nn, &elementNodes, &elementNodes_n, &elementNodes_nn,
                                 -1,  // для всех элементов
                                 -1,  // все сущности
                                 &error);

        // Подсчет треугольных элементов
        size_t totalTriElements = 0;
        for (int i = 0; i < (int)elementTypes_n; i++) {
            if (elementTypes[i] == 2) {  // только треугольники (тип 2 в Gmsh)
                totalTriElements += elementTags_n[i];
            }
        }
        fprintf(file, "%zu\n", totalTriElements);
        
        for (int i = 0; i < (int)elementTypes_n; i++) {
            if (elementTypes[i] == 2) {  // только треугольники (тип 2 в Gmsh)
                size_t nodesPerElem = elementNodes_n[i] / elementTags_n[i];
                for (size_t j = 0; j < elementTags_n[i]; j++) {
                    for (size_t k = 0; k < nodesPerElem; k++) {
                        size_t nodeIdx = elementNodes[i][j * nodesPerElem + k];
                        fprintf(file, "%zu", tagIndex[nodeIdx]);
                        if (k < nodesPerElem - 1) fprintf(file, " ");
                    }
                    fputc('\n', file);
                }
            }
        }

        fclose(file);
        free(tagIndex);

        gmshFinalize(&error);
        return (error == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    } else {
        perror("You need to add the model file");
        return EXIT_FAILURE;
    }
}

// Вспомогательная функция для получения количества элементов из сгенерированной сетки
static size_t getElementCount(int *error) {
    int *elementTypes;
    size_t elementTypes_n, elementTags_nn, elementNodes_nn;
    size_t **elementTags, **elementNodes;
    size_t *elementTags_n, *elementNodes_n;
    
    gmshModelMeshGetElements(&elementTypes, &elementTypes_n, &elementTags, &elementTags_n,
                             &elementTags_nn, &elementNodes, &elementNodes_n, &elementNodes_nn,
                             -1, -1, error);
    
    if (*error) {
        return 0;
    }
    
    // Подсчет треугольных элементов (тип 2 в Gmsh)
    size_t totalTriElements = 0;
    for (int i = 0; i < (int)elementTypes_n; i++) {
        if (elementTypes[i] == 2) {
            totalTriElements += elementTags_n[i];
        }
    }
    
    return totalTriElements;
}

// Вспомогательная функция для получения площади модели из bounding box
static double getModelArea(int *error) {
    double xmin = 0.0, ymin = 0.0, zmin = 0.0, xmax = 0.0, ymax = 0.0, zmax = 0.0;
    
    // Получаем все сущности модели и вычисляем bounding box
    int *surfaceTags = NULL;
    size_t surfaceTags_n = 0;
    gmshModelOccGetEntities(&surfaceTags, &surfaceTags_n, 2, error);  // 2 = surfaces
    
    if (*error || surfaceTags_n == 0) {
        // Если не удалось получить поверхности, пробуем получить объемы
        gmshModelOccGetEntities(&surfaceTags, &surfaceTags_n, 3, error);  // 3 = volumes
        if (*error || surfaceTags_n == 0) {
            // Если ничего не найдено, возвращаем значение по умолчанию
            return 0.0;
        }
    }
    
    // Получаем bounding box для первой сущности
    if (surfaceTags_n > 0) {
        int dim = (surfaceTags[0] < 1000) ? 2 : 3;
        int tag = surfaceTags[1];  // tag находится во втором элементе пары [dim, tag]
        gmshModelOccGetBoundingBox(dim, tag, &xmin, &ymin, &zmin, &xmax, &ymax, &zmax, error);
    }
    
    if (*error) {
        return 0.0;
    }
    
    double width = xmax - xmin;
    double height = ymax - ymin;
    return width * height;
}

// Генерация сетки с заданным количеством элементов (итеративный подход)
bool_t hyperMeshWithTargetElements(char *cadFile, int targetElements, 
                                   double *finalClmin, double *finalClmax, int *actualElements) {
    int error = 0;
    
    if (targetElements < 10) {
        fprintf(stderr, "Error: targetElements must be at least 10\n");
        return EXIT_FAILURE;
    }
    
    const char *outputNodeFile = "../build/node.txt";
    const char *outputMeshFile = "../build/HyperMesh.msh";
    
    // Инициализация Gmsh
    fprintf(stderr, "Initializing Gmsh...\n");
    gmshInitialize(1, NULL, 1, 0, &error);
    if (error) {
        fprintf(stderr, "Error initializing Gmsh: %d\n", error);
        return EXIT_FAILURE;
    }
    
    // Загрузка STEP файла
    fprintf(stderr, "Loading STEP file: %s\n", cadFile);
    gmshMerge(cadFile, &error);
    if (error) {
        fprintf(stderr, "Error loading STEP file: %d\n", error);
        gmshFinalize(&error);
        return EXIT_FAILURE;
    }
    
    // Синхронизация модели
    fprintf(stderr, "Synchronizing model...\n");
    gmshModelOccSynchronize(&error);
    if (error) {
        fprintf(stderr, "Error synchronizing model: %d\n", error);
        gmshFinalize(&error);
        return EXIT_FAILURE;
    }
    
    // Получаем площадь модели для начальной оценки
    double estimatedArea = getModelArea(&error);
    if (estimatedArea <= 0.0) {
        estimatedArea = 520.0;  // Значение по умолчанию
        fprintf(stderr, "Warning: Could not get model area, using default: %.2f\n", estimatedArea);
    } else {
        fprintf(stderr, "Model area: %.2f\n", estimatedArea);
    }
    
    // Начальная оценка clmax на основе желаемого количества элементов
    // Формула: элементы ≈ площадь / (0.433 * clmax²)
    const double elementAreaFactor = 0.433;
    double clmax = sqrt(estimatedArea / (elementAreaFactor * targetElements));
    
    // Корректирующий коэффициент для учета адаптивности Gmsh
    double correctionFactor = 1.0;
    if (targetElements < 50) {
        correctionFactor = 2.0;  // Для малого количества элементов
    } else if (targetElements < 200) {
        correctionFactor = 1.5;
    } else if (targetElements < 1000) {
        correctionFactor = 1.3;
    } else {
        correctionFactor = 1.2;
    }
    clmax *= correctionFactor;
    
    // Расчет clmin
    double clminRatio = 0.6;
    if (targetElements < 50) {
        clminRatio = 0.75;
    } else if (targetElements < 200) {
        clminRatio = 0.6;
    } else if (targetElements < 1000) {
        clminRatio = 0.4;
    } else {
        clminRatio = 0.25;
    }
    double clmin = clmax * clminRatio;
    
    // Ограничения
    if (clmin < 0.01) clmin = 0.01;
    if (clmax < clmin * 1.05) clmax = clmin * 1.05;
    if (clmax > 1000.0) clmax = 1000.0;
    
    fprintf(stderr, "Initial mesh parameters: clmin=%.4f, clmax=%.4f\n", clmin, clmax);
    fprintf(stderr, "Target elements: %d\n", targetElements);
    
    // Итеративный процесс подбора параметров
    const int maxIterations = 10;
    const double tolerance = 0.15;  // 15% отклонение допустимо
    size_t currentElements = 0;
    
    for (int iteration = 0; iteration < maxIterations; iteration++) {
        fprintf(stderr, "Iteration %d/%d: clmin=%.4f, clmax=%.4f\n", iteration + 1, maxIterations, clmin, clmax);
        
        // Очищаем предыдущую сетку
        if (iteration > 0) {
            // gmshModelMeshClear требует массив dimTags, передаем NULL для очистки всей сетки
            int *dimTags = NULL;
            size_t dimTags_n = 0;
            gmshModelMeshClear(dimTags, dimTags_n, &error);
        }
        
        // Устанавливаем параметры сетки
        gmshOptionSetNumber("Mesh.CharacteristicLengthMax", clmax, &error);
        gmshOptionSetNumber("Mesh.CharacteristicLengthMin", clmin, &error);
        
        if (error) {
            fprintf(stderr, "Error setting mesh options: %d\n", error);
            break;
        }
        
        // Генерируем сетку
        gmshModelMeshSetOrder(MESH_ORDER, &error);
        if (error) {
            fprintf(stderr, "Error setting mesh order: %d\n", error);
            break;
        }
        
        gmshModelMeshGenerate(DIMENSION, &error);
        if (error) {
            fprintf(stderr, "Error generating mesh: %d\n", error);
            break;
        }
        
        // Получаем количество элементов
        currentElements = getElementCount(&error);
        if (error) {
            fprintf(stderr, "Error getting element count: %d\n", error);
            break;
        }
        
        fprintf(stderr, "  Generated %zu elements (target: %d, ratio: %.2f)\n", 
                currentElements, targetElements, (double)currentElements / targetElements);
        
        // Проверяем, достигли ли мы цели
        if (currentElements == 0) {
            fprintf(stderr, "Error: No elements generated\n");
            break;
        }
        
        double ratio = (double)currentElements / (double)targetElements;
        if (ratio >= (1.0 - tolerance) && ratio <= (1.0 + tolerance)) {
            fprintf(stderr, "Target reached! Actual elements: %zu (ratio: %.2f)\n", currentElements, ratio);
            break;
        }
        
        // Корректируем clmax для следующей итерации
        // Формула: новый_clmax = старый_clmax * sqrt(текущие_элементы / целевые_элементы)
        // Если элементов слишком много, увеличиваем clmax (уменьшаем плотность)
        // Если элементов слишком мало, уменьшаем clmax (увеличиваем плотность)
        double adjustmentFactor = sqrt((double)targetElements / (double)currentElements);
        clmax *= adjustmentFactor;
        clmin = clmax * clminRatio;
        
        fprintf(stderr, "  Adjusting: clmax=%.4f (factor=%.2f)\n", clmax, adjustmentFactor);
        
        // Ограничения
        if (clmin < 0.01) clmin = 0.01;
        if (clmax < clmin * 1.05) clmax = clmin * 1.05;
        if (clmax > 1000.0) clmax = 1000.0;
    }
    
    // Сохраняем финальные параметры
    if (finalClmin) *finalClmin = clmin;
    if (finalClmax) *finalClmax = clmax;
    if (actualElements) *actualElements = (int)currentElements;
    
    // Сохраняем сетку в файл
    fprintf(stderr, "Writing mesh to: %s\n", outputMeshFile);
    gmshWrite(outputMeshFile, &error);
    if (error) {
        fprintf(stderr, "Error writing mesh file: %d\n", error);
        gmshFinalize(&error);
        return EXIT_FAILURE;
    }
    
    // Конвертируем в node.txt формат
    // Получаем узлы
    size_t nodeTags_n, coord_n, param_n;
    double *nodeCoords, *nodeParams;
    size_t *nodeTags;
    
    gmshModelMeshGetNodes(&nodeTags, &nodeTags_n, &nodeCoords, &coord_n, &nodeParams, &param_n, -1, 0, 0, 0, &error);
    if (error) {
        fprintf(stderr, "Error getting nodes: %d\n", error);
        gmshFinalize(&error);
        return EXIT_FAILURE;
    }
    
    // Создаем индекс для узлов
    size_t maxTag = 0;
    for (size_t i = 0; i < nodeTags_n; i++) {
        if (nodeTags[i] > maxTag) maxTag = nodeTags[i];
    }
    
    size_t *tagIndex = (size_t *)calloc(maxTag + 1, sizeof(size_t));
    for (size_t i = 0; i < nodeTags_n; i++) {
        tagIndex[nodeTags[i]] = i + 1;
    }
    
    // Записываем node.txt
    FILE *file = fopen(outputNodeFile, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not open output file: %s\n", outputNodeFile);
        free(tagIndex);
        gmshFinalize(&error);
        return EXIT_FAILURE;
    }
    
    fprintf(file, "%zu\n", nodeTags_n);
    for (size_t i = 0; i < nodeTags_n; i++) {
        fprintf(file, "%lf %lf %lf\n", nodeCoords[i * 3], nodeCoords[i * 3 + 1], nodeCoords[i * 3 + 2]);
    }
    
    // Получаем элементы
    int *elementTypes;
    size_t elementTypes_n, elementTags_nn, elementNodes_nn;
    size_t **elementTags, **elementNodes;
    size_t *elementTags_n, *elementNodes_n;
    
    gmshModelMeshGetElements(&elementTypes, &elementTypes_n, &elementTags, &elementTags_n,
                             &elementTags_nn, &elementNodes, &elementNodes_n, &elementNodes_nn,
                             -1, -1, &error);
    
    if (!error) {
        fprintf(file, "%zu\n", currentElements);
        for (int i = 0; i < (int)elementTypes_n; i++) {
            if (elementTypes[i] == 2) {  // треугольники
                size_t nodesPerElem = elementNodes_n[i] / elementTags_n[i];
                for (size_t j = 0; j < elementTags_n[i]; j++) {
                    for (size_t k = 0; k < nodesPerElem; k++) {
                        size_t nodeIdx = elementNodes[i][j * nodesPerElem + k];
                        fprintf(file, "%zu", tagIndex[nodeIdx]);
                        if (k < nodesPerElem - 1) fprintf(file, " ");
                    }
                    fprintf(file, "\n");
                }
            }
        }
    }
    
    fclose(file);
    free(tagIndex);
    
    fprintf(stderr, "Mesh generation completed: %zu elements (target: %d)\n", currentElements, targetElements);
    
    gmshFinalize(&error);
    return (error == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}