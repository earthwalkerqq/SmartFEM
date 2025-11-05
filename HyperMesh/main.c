#include <stdio.h>
#include <stdlib.h>
#include "meshGenerate.h"

int main(int argc, char **argv) {
    const char *cadFile = (argc > 1) ? argv[1] : "../../materials/data-sample/52.stp";
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

