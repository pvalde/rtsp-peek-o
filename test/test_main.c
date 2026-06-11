#include "test_harness.h"

TestContext ctx = {
    .tests_run = 0, .tests_failed = 0, .current_test_passed = true};

// Forward declarations of suite runners
void run_layout_tests(void);

int main(void) {
    printf("=========================================\n");
    printf("       STARTING RTSP-PEEK TEST SUITE     \n");
    printf("=========================================\n");

    // Execute test suites
    run_layout_tests();

    // Final Reporting
    printf("\n=========================================\n");
    if (ctx.tests_failed == 0) {
        printf(ANSI_COLOR_GREEN
               "SUCCESS: All %d tests passed successfully." ANSI_COLOR_RESET
               "\n",
               ctx.tests_run);
        printf("=========================================\n");
        return 0;
    } else {
        printf(ANSI_COLOR_RED
               "FAILURE: %d out of %d tests failed." ANSI_COLOR_RESET "\n",
               ctx.tests_failed, ctx.tests_run);
        printf("=========================================\n");
        return 1;
    }
}
