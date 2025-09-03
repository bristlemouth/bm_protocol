set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(SERIAL_BRIDGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../common/serial_bridge)

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code/user_code.cpp
    ${SERIAL_BRIDGE_DIR}/serial_bridge.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code
    ${SERIAL_BRIDGE_DIR}
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
