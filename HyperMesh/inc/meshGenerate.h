#ifndef MESH_GENERATE_H
#define MESH_GENERATE_H

#ifdef SQUARE_GRID
#define MESH_ORDER 2
#else
#define MESH_ORDER 1
#endif

#ifndef DIMENSION
#define DIMENSION 2 // по умолчанию рассматривается 2D задача
#endif

typedef enum bool_t bool_t;

void initHyperMesh(char *inputFile, char *outputMeshFile, double clmin, double clmax, int *error);
bool_t hyperMesh(char *cadFile, double clmin, double clmax);

#endif