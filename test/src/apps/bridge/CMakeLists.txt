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
