add_executable(rbr_sensor_util_tests)

target_include_directories(rbr_sensor_util_tests
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/lib/bm_core/bcmp
    ${TEST_DIR}/mocks
    ${TEST_DIR}/third_party/fff
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/sys
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${SRC_DIR}/third_party/tinycbor/src
    ${SRC_DIR}/apps/bristleback_apps/bm_rbr/user_code
    )

target_sources(rbr_sensor_util_tests
    PRIVATE
    ${SRC_DIR}/apps/bristleback_apps/bm_rbr/user_code/rbr_sensor_util.cpp
    rbr_sensor_util_ut.cpp)

target_link_libraries(rbr_sensor_util_tests gmock gtest_main)

add_test(NAME rbr_sensor_util_tests COMMAND rbr_sensor_util_tests)
