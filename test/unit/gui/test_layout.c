#include "layout.h"
#include "test_harness.h"

// We'll manually include or define SDL_Rect elements if needed,
// but assuming layout.h pulls in SDL.h, we are good to go.

static void test_set_pos_null_rect(void) {
    Layout layout = {.rows = 2, .columns = 2};
    int result = set_pos(NULL, 0, layout, 1080, 1920, 40);

    // Verify that the function returns the correct error code for NULL pointers
    EXPECT_INT_EQ(-4, result);
}

static void test_set_pos_happy_path(void) {
    SDL_Rect rect = {0};
    Layout layout = {.rows = 2, .columns = 2};

    // Test index 1 (First row, second column)
    int result = set_pos(&rect, 1, layout, 1040, 1920, 40);

    EXPECT_INT_EQ(0, result);
    // Column weight should be 1920 / 2 = 960. Index 1 means column 1.
    // EXPECT_INT_EQ(960, rect.x);
    EXPECT_INT_EQ(960, rect.x);
    EXPECT_INT_EQ(960, rect.w);
}

// This is the single public hook exposed to test_main.c
void run_layout_tests(void) {
    TESTS_HEADER
    RUN_TEST(test_set_pos_null_rect);
    RUN_TEST(test_set_pos_happy_path);
}
