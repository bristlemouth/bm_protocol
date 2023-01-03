
#
# strtof tests
#
add_executable(util_strtof)
target_include_directories(util_strtof
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/nmea
    ${SRC_DIR}/third_party/FreeRTOS/Source/include

)

target_sources(util_strtof
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/util.c

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Unit test wrapper for test
    strtof_ut.cpp
)

target_link_libraries(util_strtof gtest gmock gtest_main)

add_test(
  NAME
    util_strtof
  COMMAND
    util_strtof
  )

#
# strtod tests
#
add_executable(util_strtod)
target_include_directories(util_strtod
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/nmea
    ${SRC_DIR}/third_party/FreeRTOS/Source/include

)

target_sources(util_strtod
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/util.c

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Unit test wrapper for test
    strtod_ut.cpp
)

target_link_libraries(util_strtod gtest gmock gtest_main)

add_test(
  NAME
    util_strtod
  COMMAND
    util_strtod
  )

#
# TimeRemaining tests
#
add_executable(util_timeRemaining)
target_include_directories(util_timeRemaining
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/nmea
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
)

target_sources(util_timeRemaining
    PRIVATE
    # File we're testing
    ${SRC_DIR}/lib/common/util.c

    # Stubs
    ${TEST_DIR}/stubs/FreeRTOSStubs.c

    # Unit test wrapper for test
    timeRemaining_ut.cpp
)

target_link_libraries(util_timeRemaining gtest gmock gtest_main)

add_test(
  NAME
    util_timeRemaining
  COMMAND
    util_timeRemaining
  )

#
# utcFromDateTime tests
#
add_executable(utcFromDateTime)
target_include_directories(utcFromDateTime
    PRIVATE
    ${TEST_DIR}/header_overrides
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/third_party/FreeRTOS/Source/include
    ${SRC_DIR}/third_party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    ${SRC_DIR}/lib/WSLib
)

target_sources(utcFromDateTime
    PRIVATE

    # File we're testing
    ${SRC_DIR}/lib/common/util.c

    # Unit test wrapper for test
    utcFromDateTime_ut.cpp
)

target_link_libraries(utcFromDateTime gtest gmock gtest_main)

add_test(
  NAME
    utcFromDateTime
  COMMAND
    utcFromDateTime
  )
