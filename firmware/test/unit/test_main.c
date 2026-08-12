#include "test_framework.h"

#include <stdio.h>
#include <stdlib.h>

int helm_test_failures = 0;
int helm_test_count = 0;

void test_pid_step_response(void);
void test_pid_output_clamped(void);
void test_plausibility_raises_at_exact_fault_cycle_count(void);
void test_plausibility_clears_at_exact_clear_cycle_count(void);
void test_plausibility_no_false_positives_on_healthy_pair(void);
void test_plausibility_intermittent_bad_cycles_do_not_accumulate(void);

int main(void) {
    test_pid_step_response();
    test_pid_output_clamped();
    test_plausibility_raises_at_exact_fault_cycle_count();
    test_plausibility_clears_at_exact_clear_cycle_count();
    test_plausibility_no_false_positives_on_healthy_pair();
    test_plausibility_intermittent_bad_cycles_do_not_accumulate();

    printf("\n%d/%d checks passed\n", helm_test_count - helm_test_failures, helm_test_count);
    if (helm_test_failures > 0) {
        printf("FAILED\n");
        return EXIT_FAILURE;
    }
    printf("ALL PASSED\n");
    return EXIT_SUCCESS;
}
