
mark_as_advanced(
    BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
    gmock_build_tests gtest_build_samples gtest_build_tests
    gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)

set_target_properties(gtest PROPERTIES FOLDER extern)
set_target_properties(gtest_main PROPERTIES FOLDER extern)
set_target_properties(gmock PROPERTIES FOLDER extern)
set_target_properties(gmock_main PROPERTIES FOLDER extern)

add_compile_options(
    -Wall
    -Wextra
    -Werror
    -g
    -ggdb
    -Og
    -DCI_TEST
    -DCBOR_CUSTOM_ALLOC_INCLUDE="tinycbor_alloc.h"
    -DCBOR_PARSER_MAX_RECURSIONS=10
    -DGTEST_BREAK_ON_FAILURE=1
)

add_compile_definitions(ENABLE_TESTING)
add_subdirectory("src")
