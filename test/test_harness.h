#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdbool.h>
#include <stdio.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET "\x1b[0m"

// Global context
typedef struct {
    int tests_run;
    int tests_failed;
    bool current_test_passed;
} TestContext;

extern TestContext ctx;

// Custom non-aborting assertion macros
#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (!(expr)) {                                                         \
            printf(ANSI_COLOR_RED "[FAIL] " ANSI_COLOR_RESET                   \
                                  " %s:%d: Expected '%s' to be true\n",        \
                   __FILE__, __LINE__, #expr);                                 \
            ctx.current_test_passed = false;                                   \
        }                                                                      \
    } while (0)

#define EXPECT_FALSE(expr)                                                     \
    do {                                                                       \
        if ((expr)) {                                                          \
            printf(ANSI_COLOR_RED "[FAIL] " ANSI_COLOR_RESET                   \
                                  " %s:%d: Expected '%s' to be false\n",       \
                   __FILE__, __LINE__, #expr);                                 \
            ctx.current_test_passed = false;                                   \
        }                                                                      \
    } while (0)

#define EXPECT_INT_EQ(expected, actual)                                        \
    do {                                                                       \
        int exp_val = (expected);                                              \
        int act_val = (actual);                                                \
        if (exp_val != act_val) {                                              \
            printf(ANSI_COLOR_RED "[FAIL] " ANSI_COLOR_RESET                   \
                                  " %s:%d: Expected %d, but got %d\n",         \
                   __FILE__, __LINE__, exp_val, act_val);                      \
            ctx.current_test_passed = false;                                   \
        }                                                                      \
    } while (0)

// Function wrapper

#define RUN_TEST(test_func)                                                    \
    do {                                                                       \
        printf("Running %s...\n", #test_func);                                 \
        ctx.current_test_passed = true;                                        \
        ctx.tests_run++;                                                       \
        test_func();                                                           \
        if (!ctx.current_test_passed) {                                        \
            ctx.tests_failed++;                                                \
        }                                                                      \
    } while (0)

#define TESTS_HEADER printf("\n=== " __FILE__ " ===\n");

#endif // TEST_HARNESS_H
