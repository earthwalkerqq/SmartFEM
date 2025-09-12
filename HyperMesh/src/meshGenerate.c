#include <gmshc.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "meshGenerate.h"

enum {
    FALSE = 0;
    TRUE
} bool_t;

void initHyperMesh(char *inputFile, char *outputMeshFile, double clmin, double clmax, int *error) {
    if (!gmshIsInitialized(error)) gmshInitialize(1, NULL, 1, 0, error);

    if (!*error) {
        gmshMerge(inputFile, error);

        // Синхронизация CAD-модели в ядро Gmsh
        gmshOptionSetNumber("Mesh.CharacteristicLengthMax", clmax, error);
        gmshOptionSetNumber("Mesh.CharacteristicLengthMin", clmin, error);

        gmshModelMeshSetOrder(MESH_ORDER, error);

        gmshModelMeshGenerate(DIMENTION, error);  // dim - заданное пространство

        gmshWrite(outputMeshFile, error);
    }
}

bool_t hyperMesh(char *cadFile, double clmin, double clmax) {
    bool_t error = EXIT_SUCCESS;

    const char *ouputNodeFile = "../build/node.txt";
    const char *outputMeshFile = "../build/HyperMesh.msh";

    if (cadFile != NULL) {
        initHyperMesh(cadFile, outputMeshFile, clmin, clmax, &error);

        assert(!error);
        
        gmshModelAdd("model", &error);

        size_t nodeTags_n, coord_n, param_n;
        double *nodeCoords, *nodeParams;
        size_t *nodeTags;

        gmshModelMeshGetNodes(&nodeTags, &nodeTags_n, &nodeCoords, &coord_n, &nodeParams, &param_n, -1, 0, 0,
                              0, &error);  // dim = -1 (все элементы)

        size_t maxTag = *nodeTags;
        for (int i = 0; i < nodeTags_n; i++) {
            if (nodeTags[i] > maxTag) maxTag = nodeTags[i];
        }

        size_t *tagIndex = (size_t *)calloc(maxTag + 1, sizeof(size_t));
        for (int i = 0; i < nodeTags_n; i++) {
            tagIndex[nodeTags[i]] = i + 1;
        }

        FILE *file = fopen(ouputFile, "w");

        assert(file != NULL);

        fprintf(file, "%zu\n", nodeTags_n);

        for (int i = 0; i < nodeTags_n; i++) {
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
                                 NULL);

        /*------------------------*/
        /* необходимо исправить!  */
        /*------------------------*/
        fprintf(file, "%zu\n", elementTags_nn);
        for (int i = 0; i < elementTypes_n; i++) {
            if (elementTypes[i] == 2) {  // только треугольники
                size_t countNodeInElem = elementNodes_n[i] / elementTags_n[i];
                for (int j = 0; j < elementTags_n[i]; j++) {
                    for (int k = 0; k < countNodeInElem; k++) {
                        (k < countNodeInElem - 1)
                            ? fprintf(file, "%zu ", tagIndex[elementNodes[i][j * countNodeInElem + k]])
                            : fprintf(file, "%zu", tagIndex[elementNodes[i][j * countNodeInElem + k]]);
                    }
                    fputc('\n', file);
                }
            }
        }

        fclose(file);
        free(tagIndex);

        gmshFinalize(&ierr);
    } else {
        ierr = EXIT_FAILURE;
        perror("You need to add the model file");
    }

    return ierr;
}