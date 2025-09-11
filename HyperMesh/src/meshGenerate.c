#include <stdio.h>
#include <stdlib.h>
#include <gmshc.h>

#include "meshGenerate.h"

int hyperMesh(char *cadFile)
{
    int ierr = EXIT_SUCCESS;
    
    if (!gmshIsInitialized(ierr)) 
        gmshInitialize(argc, argv, 1, 0, &ierr);

    gmshModelAdd("model", &ierr);

    if (cadFile != NULL) {
        printf("Загружаем STEP файл: %s\n", argv[1]);
        gmshMerge(argv[1], &ierr);

    // Синхронизация CAD-модели в ядро Gmsh
    gmshOptionSetNumber("Mesh.CharacteristicLengthMax", 100, &ierr);
    gmshOptionSetNumber("Mesh.CharacteristicLengthMin", 90, &ierr);

    gmshModelMeshSetOrder(2, &ierr);

    gmshModelMeshGenerate(3, &ierr); // dimension = 3

    gmshWrite("HyperMesh.msh", &ierr);

    size_t nodeTags_n, coord_n, param_n;
    double *nodeCoords, *nodeParams;
    size_t *nodeTags;

    gmshModelMeshGetNodes(&nodeTags, &nodeTags_n, &nodeCoords, &coord_n,
                          &nodeParams, &param_n,  -1, 0, 0, 0, &ierr); // dim = -1 (все элементы)

    size_t maxTag = *nodeTags;
    for (int i = 0; i < nodeTags_n; i++) {
      if (nodeTags[i] > maxTag) maxTag = nodeTags[i];
    }

    size_t *tagIndex = (size_t*)calloc(maxTag + 1, sizeof(size_t));
    for (int i = 0; i < nodeTags_n; i++) {
      tagIndex[nodeTags[i]] = i + 1;
    }


    char *filename = "node.txt";
    FILE *file = fopen(filename, "w");

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

    gmshModelMeshGetElements(&elementTypes, &elementTypes_n,
                            &elementTags, &elementTags_n, &elementTags_nn,
                            &elementNodes, &elementNodes_n, &elementNodes_nn,
                            -1, // для всех элементов
                            -1, // все сущности
                            NULL);

    fprintf(file, "%zu\n", elementTags_nn);
    for (int i = 0; i < elementTypes_n; i++) {
      if (elementTypes[i] == 2) { // только треугольники
        size_t countNodeInElem = elementNodes_n[i] / elementTags_n[i];
        for (int j = 0; j < elementTags_n[i]; j++) {
          for (int k = 0; k < countNodeInElem; k++) {
            (k < countNodeInElem - 1) ? fprintf(file, "%zu ", tagIndex[elementNodes[i][j * countNodeInElem + k]])
                                      : fprintf(file, "%zu", tagIndex[elementNodes[i][j * countNodeInElem + k]]);
          }
          fputc('\n', file);
        }
      }
    }

    fclose(file);
    free(tagIndex);

    gmshFinalize(&ierr);
    return 0;
  }

  // ----------------------------
  // Иначе выполняется старая часть (ручное построение геометрии)
  // ----------------------------
  const double lc = 1e-2; // размер КЭ
  gmshModelGeoAddPoint(0, 0, 0, lc, 1, &ierr);
  gmshModelGeoAddPoint(.1, 0, 0, lc, 2, &ierr);
  gmshModelGeoAddPoint(.1, .3, 0, lc, 3, &ierr);
  gmshModelGeoAddPoint(0, .3, 0, lc, 4, &ierr);

  gmshModelGeoAddLine(1, 2, 1, &ierr);
  gmshModelGeoAddLine(3, 2, 2, &ierr);
  gmshModelGeoAddLine(3, 4, 3, &ierr);
  gmshModelGeoAddLine(4, 1, 4, &ierr);

  const int cl1[] = {4, 1, -2, 3};
  gmshModelGeoAddCurveLoop(cl1, sizeof(cl1) / sizeof(cl1[0]), 1, 0, &ierr);

  const int s1[] = {1};
  gmshModelGeoAddPlaneSurface(s1, sizeof(s1) / sizeof(s1[0]), 1, &ierr);

  gmshModelGeoSynchronize(&ierr);

  gmshModelMeshGenerate(2, &ierr);
  gmshWrite("t2.msh", &ierr);

  gmshFinalize(&ierr);

  return ierr;
}