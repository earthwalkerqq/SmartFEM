#ifndef TEST_FEM_H
#define TEST_FEM_H

#include <check.h>
#include <stdlib.h>

#include "../fem.h"
#include "../formation_mtrx.h"

Suite *test_formatationDeformMtrx(void);
Suite *test_formatationElastMtrx(void);
Suite *test_formatationPlaneElem(void);
Suite *test_formatationStressMtrx(void);
Suite *test_formatationGlobalMtrx(void);
Suite *test_assemblGlobalMtrx(void);

#endif