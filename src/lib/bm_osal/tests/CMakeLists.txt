# tests/CMakeLists.txt

add_executable(test_osal_mock
    test_osal_mock.cpp
    ${LIB_DIR}/bm_osal/bm_osal_mock.c
)

target_include_directories(test_osal_mock
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/lib
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(test_osal_mock
    gtest gtest_main pthread
)

add_test(
  NAME
    bm_osal_tests
  COMMAND
    bm_osal_tests
  )
