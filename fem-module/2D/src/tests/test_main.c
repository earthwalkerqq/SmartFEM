#include <stdio.h>

#include "test_fem.h"

int main(void) {
    int failed = 0;
    Suite *fem_test[] = {test_formatationDeformMtrx(), test_formatationElastMtrx(),
                         test_formatationStressMtrx(), test_formatationPlaneElem(),
                         test_formatationGlobalMtrx(), test_assemblGlobalMtrx()};
    for (int i = 0; fem_test[i] != NULL; i++) {
        SRunner *sr = srunner_create(fem_test[i]);
        srunner_run_all(sr, CK_NORMAL);
        failed += srunner_ntests_failed(sr);
        srunner_free(sr);

        if (!failed) {
            printf("Success");
        } else {
            printf("Failed");
        }
        failed = 0;
    }
    return 0;
}