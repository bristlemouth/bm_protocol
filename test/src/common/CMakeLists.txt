
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
# Lib State Machine
#
add_executable(lib_state_machine_tests)
target_include_directories(lib_state_machine_tests
    PRIVATE
    ${SRC_DIR}/lib/common
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
)

target_sources(lib_state_machine_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/lib_state_machine.cpp

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Unit test wrapper for test
    lib_state_machine_ut.cpp
)

target_link_libraries(lib_state_machine_tests gtest gmock gtest_main)

add_test(
  NAME
    lib_state_machine_tests
  COMMAND
    lib_state_machine_tests
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
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/sys
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
