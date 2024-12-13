set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../rs232_expander_apps_common/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code/user_code.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code/rbr_sensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code/rbr_sensor_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../rs232_expander_apps_common/sensors/sensors.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../rs232_expander_apps_common
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code
    ${CMAKE_CURRENT_SOURCE_DIR}/../rs232_expander_apps_common/sensors
)

set(APP_LIBS
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    # Make sure trace.c is included in the build above
    # Uncomment/change these to enable tracing! (see trace.h for more options)
    # TRACE_ENABLE
    # TRACE_BUFF_LEN=1024
    # TRACE_TASKS
    # TRACE_USE_COREDEBUG
    # TRACE_ISR
    # TRACE_SERIAL

    # Add network stress test functions
    STRESS_TEST_ENABLE
    )

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
