#include <gmshc.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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