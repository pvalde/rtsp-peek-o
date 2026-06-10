#include "stream_manager_internal.h"
#include "test_harness.h"

static void test_managed_stream_destroy_frees_resources(void) {
    struct ManagedStream *ms = managed_stream_create();

    EXPECT_TRUE(ms != NULL);

    managed_stream_destroy(&ms);

    EXPECT_TRUE(ms == NULL);
}

void run_managed_stream_tests(void) {
    TESTS_HEADER
    RUN_TEST(test_managed_stream_destroy_frees_resources);
}
