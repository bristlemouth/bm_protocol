set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pps_timer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code/user_code.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors/sensors.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors
)

set(APP_LIBS
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    # Add network stress test functions
    STRESS_TEST_ENABLE
    # Enable ADIN2111 1 PPS TS_TIMER output on P1_LED_0 (bm_core)
    bm_adin2111_ts_timer_enabled=1
    )

# PPS input filter on TIM2, disabled unless set (see pps_timer.h)
if (DEFINED PPS_TIMER_INPUT_FILTER)
list(APPEND APP_DEFINES "PPS_TIMER_INPUT_FILTER=${PPS_TIMER_INPUT_FILTER}")
message(STATUS "PPS TIMER INPUT FILTER ${PPS_TIMER_INPUT_FILTER}")
endif()
if (DEFINED PPS_TIMER_INPUT_FILTER_CKD)
list(APPEND APP_DEFINES "PPS_TIMER_INPUT_FILTER_CKD=${PPS_TIMER_INPUT_FILTER_CKD}")
endif()

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
