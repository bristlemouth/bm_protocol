#include "bm_osal.h"
#include "bm_osal_mock_defs.h"
#include <gtest/gtest.h>

// ====================================================
// Test Mocked OSAL Functions
// - Only doing very simple test for task creation and destruction
// - This is a mock implementation, so it does not test real FreeRTOS behavior
// ====================================================

TEST(OSALMockTest, TaskCreateAndDestroy) {
    bm_osal_task_t task;
    bm_osal_task_handle_t handle = NULL;

    auto dummy_fn = [](void* arg) -> void { *(int*)arg = 123; };
    int arg = 0;

    EXPECT_TRUE(bm_osal_task_create(&task, dummy_fn, &arg, "dummy", 128, &handle));
    EXPECT_EQ(handle, BM_OSAL_DUMMY_TASK_HANDLE);

    EXPECT_EQ(bm_osal_task_get_min_stack_size(), 128);

    EXPECT_TRUE(bm_osal_task_destroy(&task));
}
