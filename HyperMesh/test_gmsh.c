#include <gmshc.h>
#include <stdio.h>

int main() {
    int error = 0;
    printf("Testing Gmsh initialization...\n");
    fflush(stdout);
    
    gmshInitialize(1, NULL, 1, 0, &error);
    if (error) {
        printf("Error initializing: %d\n", error);
        return 1;
    }
    
    printf("Gmsh initialized successfully!\n");
    
    gmshFinalize(&error);
    printf("Done.\n");
    return 0;
}

