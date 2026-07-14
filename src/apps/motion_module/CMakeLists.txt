set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# IMU driver selection: pass -DIMU=BNO085 (default LSM6DSV)
if(NOT DEFINED IMU)
    set(IMU LSM6DSV)
endif()

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SRC_DIR}/lib/drivers/
)

set(APP_LIBS
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    # Add network stress test functions
    BOREALIS_MOTE_GIT_HASH="${GIT_HASH}"
)

if(IMU STREQUAL "BNO085")
    list(APPEND APP_FILES
        ${SRC_DIR}/third_party/ceva_sh2/sh2.c
        ${SRC_DIR}/third_party/ceva_sh2/shtp.c
        ${SRC_DIR}/third_party/ceva_sh2/sh2_SensorValue.c
        ${SRC_DIR}/third_party/ceva_sh2/sh2_util.c
        ${SRC_DIR}/lib/drivers/bno085.cpp
    )
    list(APPEND APP_INCLUDES ${SRC_DIR}/third_party/ceva_sh2/)
    list(APPEND APP_DEFINES IMU_BNO085=1)
endif()


# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
