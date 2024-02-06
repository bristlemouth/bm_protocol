function(create_gtest library_name function_to_test)
    set(GTEST_NAME ${library_name}_${function_to_test})
    add_executable(${GTEST_NAME})
    target_sources(${GTEST_NAME}
            PRIVATE
            # Files we're testing
            ${SRC_FILES}

            # Stubs
            ${TEST_STUBS}

            # Unit test wrapper for test
            ${function_to_test}_ut.cpp
    )
    add_test(
        NAME ${GTEST_NAME}
        COMMAND ${GTEST_NAME}
    )

    target_compile_options(${GTEST_NAME}
        PRIVATE
        -Wall
        -Wextra
        -Werror
    )
endfunction()

add_subdirectory("common")
add_subdirectory("drivers")
add_subdirectory("apps/bridge")
add_subdirectory("apps/bm_rbr")
