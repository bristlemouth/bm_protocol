add_executable(aanderaa_conductivity_sensor_util_tests)

target_include_directories(aanderaa_conductivity_sensor_util_tests
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
    ${SRC_DIR}/apps/bristleback_apps/aanderaa_conductivity/user_code
    ${SRC_DIR}/apps/rs232_expander_apps/aanderaa_conductivity/user_code
    )

target_sources(aanderaa_conductivity_sensor_util_tests
    PRIVATE
    ${SRC_DIR}/apps/bristleback_apps/aanderaa_conductivity/user_code/aanderaa_conductivity_sensor_util.cpp
    aanderaa_conductivity_sensor_util_ut.cpp)

target_link_libraries(aanderaa_conductivity_sensor_util_tests gmock gtest_main)

add_test(NAME aanderaa_conductivity_sensor_util_tests COMMAND aanderaa_conductivity_sensor_util_tests)
set_tests_properties(aanderaa_conductivity_sensor_util_tests PROPERTIES LABELS "conductivity;unit")
