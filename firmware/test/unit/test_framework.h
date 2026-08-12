#ifndef HELM_TEST_FRAMEWORK_H
#define HELM_TEST_FRAMEWORK_H

#include <stdio.h>

/*
 * Minimal, dependency-free test framework for host-side unit tests.
 * No RTOS, no hardware, no vendored test library -- just assert-and-report.
 */

extern int helm_test_failures;
extern int helm_test_count;

#define HELM_CHECK(cond, msg)                                                \
    do {                                                                     \
        helm_test_count++;                                                   \
        if (!(cond)) {                                                       \
            helm_test_failures++;                                            \
            printf("  FAIL: %s (%s:%d) -- %s\n", #cond, __FILE__, __LINE__,  \
                   (msg));                                                   \
        }                                                                    \
    } while (0)

#define HELM_CHECK_FLOAT_LE(val, limit, msg)                                 \
    do {                                                                     \
        helm_test_count++;                                                   \
        if (!((val) <= (limit))) {                                           \
            helm_test_failures++;                                            \
            printf("  FAIL: %s (%.4f) <= %s (%.4f) (%s:%d) -- %s\n", #val,   \
                   (double)(val), #limit, (double)(limit), __FILE__,         \
                   __LINE__, (msg));                                         \
        }                                                                    \
    } while (0)

#define HELM_CHECK_INT_EQ(val, expected, msg)                                \
    do {                                                                     \
        helm_test_count++;                                                   \
        if ((val) != (expected)) {                                           \
            helm_test_failures++;                                            \
            printf("  FAIL: %s (%d) == %s (%d) (%s:%d) -- %s\n", #val,       \
                   (int)(val), #expected, (int)(expected), __FILE__,         \
                   __LINE__, (msg));                                         \
        }                                                                    \
    } while (0)

#endif /* HELM_TEST_FRAMEWORK_H */
