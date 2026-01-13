set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(APP_FILES
    ${SRC_DIR}/third_party/ceva_sh2/sh2.c
    ${SRC_DIR}/third_party/ceva_sh2/shtp.c
    ${SRC_DIR}/third_party/ceva_sh2/sh2_SensorValue.c
    ${SRC_DIR}/third_party/ceva_sh2/sh2_util.c
    ${SRC_DIR}/lib/drivers/bno085.cpp
    ${CMAKE_CURRENT_LIST_DIR}/user_code/user_code.cpp
)

set(APP_INCLUDES
    ${SRC_DIR}/lib/drivers/
    ${SRC_DIR}/third_party/ceva_sh2/
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/user_code
)

set(APP_LIBS
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    # Add network stress test functions
    STRESS_TEST_ENABLE
)


# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
