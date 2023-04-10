#
# INA232 tests
#
add_executable(ina232_tests)
target_include_directories(ina232_tests
    PRIVATE
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/drivers/protected
    ${SRC_DIR}/lib/drivers/
    ${SRC_DIR}/lib/common/
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${TEST_DIR}/third_party/fff/
)

target_sources(ina232_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/drivers/ina232.cpp

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Helpers
    ${SRC_DIR}/lib/common/util.c
    ${SRC_DIR}/lib/drivers/abstract/abstract_i2c.cpp

    # Unit test wrapper for test
    ina232_ut.cpp
)

target_link_libraries(ina232_tests gtest gmock gtest_main)

add_test(
  NAME
    ina232_tests
  COMMAND
    ina232_tests
  )

#
# TCA9546A tests
#
add_executable(tca9546a_tests)
target_include_directories(tca9546a_tests
    PRIVATE
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/drivers/protected
    ${SRC_DIR}/lib/drivers/
    ${SRC_DIR}/lib/common/
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${TEST_DIR}/third_party/fff/
)

target_sources(tca9546a_tests
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/drivers/tca9546a.cpp

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Helpers
    ${SRC_DIR}/lib/common/util.c
    ${SRC_DIR}/lib/drivers/abstract/abstract_i2c.cpp

    # Unit test wrapper for test
    tca9546a_ut.cpp
)

target_link_libraries(tca9546a_tests gtest gmock gtest_main)

add_test(
  NAME
    tca9546a_tests
  COMMAND
    tca9546a_tests
  )
