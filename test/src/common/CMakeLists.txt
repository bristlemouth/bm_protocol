
add_subdirectory("util")

#
# Tokenize tests
#
add_executable(tokenize_tests)
target_include_directories(tokenize_tests
    PRIVATE
    ${SRC_DIR}/lib/common
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
)

target_sources(tokenize_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/tokenize.c

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Unit test wrapper for test
    tokenize_ut.cpp
)

target_link_libraries(tokenize_tests gtest gmock gtest_main)

add_test(
  NAME
    tokenize_tests
  COMMAND
    tokenize_tests
  )

#
# NVM Partition
#
add_executable(nvm_partition_tests)
target_include_directories(nvm_partition_tests
    PRIVATE
    ${SRC_DIR}/lib/common
    ${TEST_DIR}/header_overrides
    ${TEST_DIR}/mocks
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${SRC_DIR}/lib/bm_core/bcmp
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/sys
    ${SRC_DIR}/apps/bringup
)

target_sources(nvm_partition_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/nvmPartition.cpp

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Unit test wrapper for test
    nvmPartition_ut.cpp
)

target_link_libraries(nvm_partition_tests gtest gmock gtest_main)

add_test(
  NAME
    nvm_partition_tests
  COMMAND
    nvm_partition_tests
  )

#
# Configuration
#
add_executable(configuration_tests)
target_include_directories(configuration_tests
    PRIVATE
    ${SRC_DIR}/lib/common
    ${TEST_DIR}/header_overrides
    ${TEST_DIR}/mocks
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${SRC_DIR}/apps/bringup
    ${SRC_DIR}/lib/bm_core/bcmp
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/sys
    ${SRC_DIR}/third_party/tinycbor/src
    ${SRC_DIR}/third_party/crc
    ${TEST_DIR}/third_party/fff

)

target_sources(configuration_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/nvmPartition.cpp
    ${SRC_DIR}/lib/sys/configuration.cpp
    ${SRC_DIR}/lib/sys/ram_partitions.c
    ${SRC_DIR}/third_party/tinycbor/src/cborparser.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder_float.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder.c
    ${SRC_DIR}/third_party/tinycbor/src/cborerrorstrings.c
    ${SRC_DIR}/third_party/tinycbor/src/cborvalidation.c
    ${SRC_DIR}/third_party/crc/crc32.c

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c
    ${TEST_DIR}/stubs/mock_reset_reason.cpp

    # Unit test wrapper for test
    configuration_ut.cpp
)

target_link_libraries(configuration_tests gtest gmock gtest_main)

add_test(
  NAME
    configuration_tests
  COMMAND
    configuration_tests
  )

#
# Bridge Power Controller
#

add_executable(bridge_power_controller_tests)
target_include_directories(bridge_power_controller_tests
    PRIVATE
    ${SRC_DIR}/lib/common
    ${TEST_DIR}/header_overrides
    ${TEST_DIR}/mocks
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${TEST_DIR}/third_party/fff
    ${SRC_DIR}/lib/sys
    ${SRC_DIR}/lib/drivers
    ${SRC_DIR}/lib/bridge
    ${SRC_DIR}/lib/bcmp/bm
    ${SRC_DIR}/apps/bridge
    ${SRC_DIR}/apps/bridge/sensor_drivers
    ${SRC_DIR}/lib/bm_core/bcmp
    ${SRC_DIR}/lib/bm_core/network
    ${SRC_DIR}/lib/bm_core/drivers/adin2111
    ${SRC_DIR}/lib/bm_core/common
    ${SRC_DIR}/lib/bm_core/bm_common_messages
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/third_party/tinycbor/src
    ${SRC_DIR}/lib/bm_core/test/mocks
)

target_sources(bridge_power_controller_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/apps/bridge/bridgePowerController.cpp

    # Support files
    ${SRC_DIR}/lib/common/util.c

    # Mocks
    ${TEST_DIR}/stubs/mock_FreeRTOS.c
    ${TEST_DIR}/stubs/mock_stm32_rtc.cpp
    ${TEST_DIR}/stubs/mock_bm_serial.cpp
    ${TEST_DIR}/stubs/mock_device_info.cpp
    ${TEST_DIR}/stubs/mock_bridgeLog.cpp
    ${TEST_DIR}/stubs/mock_sensorController.cpp
    ${SRC_DIR}/lib/bm_core/test/stubs/l2_stub.c

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c


    # Unit test wrapper for test
    bridgePowerController_ut.cpp
)

target_link_libraries(bridge_power_controller_tests gtest gmock gtest_main)

add_test(
  NAME
    bridge_power_controller_tests
  COMMAND
    bridge_power_controller_tests
  )


#
# avgSampler tests
#
add_executable(avgSampler)
target_include_directories(avgSampler
    PRIVATE
    ${SRC_DIR}/lib/common
    ${TEST_DIR}/header_overrides
    ${TEST_DIR}/mocks
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${TEST_DIR}/third_party/fff
)

target_sources(avgSampler
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/avgSampler.cpp

    # Support files
    ${SRC_DIR}/lib/common/util.c

    # Unit test wrapper for test
    avgSampler_ut.cpp
)

target_link_libraries(avgSampler gtest gmock gtest_main)

add_test(
  NAME
    avgSampler
  COMMAND
    avgSampler
  )

#
# differenceSignal tests
#
add_executable(differenceSignal)
target_include_directories(differenceSignal
    PRIVATE
    ${SRC_DIR}/lib/common
    ${TEST_DIR}/header_overrides
    ${TEST_DIR}/mocks
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${TEST_DIR}/third_party/fff
)

target_sources(differenceSignal
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/differenceSignal.cpp

    # Support files

    # Mock
    ${TEST_DIR}/stubs/mock_FreeRTOS.c

    # Unit test wrapper for test
    differenceSignal_ut.cpp
)

target_link_libraries(differenceSignal gtest gmock gtest_main)

add_test(
  NAME
    differenceSignal
  COMMAND
    differenceSignal
  )
