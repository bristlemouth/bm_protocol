add_executable(sm_config_crc_list_tests)

target_include_directories(sm_config_crc_list_tests
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/lib/bm_core/bcmp
    ${SRC_DIR}/lib/bm_core/network
    ${SRC_DIR}/lib/bm_core/common
    ${TEST_DIR}/mocks
    ${TEST_DIR}/third_party/fff
    ${SRC_DIR}/apps/bridge
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/sys
    ${SRC_DIR}/third_party/crc
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${SRC_DIR}/third_party/tinycbor/src)

target_sources(sm_config_crc_list_tests
    PRIVATE
    ${SRC_DIR}/apps/bridge/sm_config_crc_list.cpp
    ${SRC_DIR}/lib/common/nvmPartition.cpp
    ${SRC_DIR}/third_party/crc/crc32.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder.c
    ${SRC_DIR}/third_party/tinycbor/src/cborparser.c
    ${SRC_DIR}/lib/bm_core/bcmp/configuration.c
    ${TEST_DIR}/stubs/mock_reset_reason.cpp
    sm_config_crc_list_ut.cpp)

target_link_libraries(sm_config_crc_list_tests gmock gtest_main)

add_test(NAME sm_config_crc_list_tests COMMAND sm_config_crc_list_tests)

add_executable(report_builder_tests)
target_include_directories(report_builder_tests
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/apps/bridge
    ${SRC_DIR}/apps/bridge/sensor_drivers
    ${SRC_DIR}/lib/bm_core/bcmp
    ${SRC_DIR}/lib/bm_core/bm_common_messages
    ${SRC_DIR}/lib/bm_core/common
    ${SRC_DIR}/lib/bm_core/middleware
    ${SRC_DIR}/lib/bm_core/test
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${TEST_DIR}/mocks
    ${TEST_DIR}/third_party/fff
    ${SRC_DIR}/third_party/tinycbor/src
)

target_sources(report_builder_tests
    PRIVATE
    ${TEST_DIR}/stubs/mock_bridgeLog.cpp
    ${SRC_DIR}/apps/bridge/reportBuilderList.cpp
    report_builder_ut.cpp)

target_link_libraries(report_builder_tests gmock gtest_main)

add_test(NAME report_builder_tests COMMAND report_builder_tests)
set_tests_properties(report_builder_tests PROPERTIES LABELS "conductivity;bridge;unit")

# Conductivity Integration Tests
add_executable(conductivity_integration_tests)
target_include_directories(conductivity_integration_tests
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/apps/bridge
    ${SRC_DIR}/apps/bridge/sensor_drivers
    ${SRC_DIR}/lib/bm_core/bcmp
    ${SRC_DIR}/lib/bm_core/bm_common_messages
    ${SRC_DIR}/lib/bm_core/common
    ${SRC_DIR}/lib/bm_core/middleware
    ${SRC_DIR}/lib/bm_core/test
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${TEST_DIR}/mocks
    ${TEST_DIR}/third_party/fff
    ${SRC_DIR}/third_party/tinycbor/src
)

target_sources(conductivity_integration_tests
    PRIVATE
    ${TEST_DIR}/stubs/mock_bridgeLog.cpp
    ${SRC_DIR}/apps/bridge/reportBuilderList.cpp
    ${SRC_DIR}/lib/bm_core/bm_common_messages/aanderaa_conductivity_msg.cpp
    ${SRC_DIR}/lib/bm_core/bm_common_messages/sensor_header_msg.cpp
    ${SRC_DIR}/lib/bm_core/bm_common_messages/sensor_header_msg.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder.c
    ${SRC_DIR}/third_party/tinycbor/src/cborparser.c
    conductivity_integration_ut.cpp)

target_link_libraries(conductivity_integration_tests gmock gtest_main)

add_test(NAME conductivity_integration_tests COMMAND conductivity_integration_tests)
set_tests_properties(conductivity_integration_tests PROPERTIES LABELS "conductivity;integration")
