set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/crc32c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/frame_codec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/uart_network_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/composite_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors/sensors.cpp
    ${SRC_DIR}/lib/common/payload_uart.cpp
    # bristlefin.cpp is not auto-included for this APP_NAME/APP_TYPE combination;
    # add it explicitly so Bristlefin::setGpioDefault() / setLed() are available.
    ${SRC_DIR}/lib/bristlefin/bristlefin.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors
)

set(APP_LIBS
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
)

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)

