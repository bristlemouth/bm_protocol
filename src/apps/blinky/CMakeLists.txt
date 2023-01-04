set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# Include FreeRTOS
add_subdirectory(${SRC_DIR}/third_party/FreeRTOS freertos_build)

# Include memfault
include(${SRC_DIR}/third_party/memfault.cmake)

set(LIB_FILES
    ${SRC_DIR}/lib/cli/cli.c
    ${SRC_DIR}/lib/common/bootloader_helper.c
    ${SRC_DIR}/lib/common/device_info.c
    ${SRC_DIR}/lib/common/enumToStr.c
    ${SRC_DIR}/lib/common/freertos_support.c
    ${SRC_DIR}/lib/common/lpm_u5.c
    ${SRC_DIR}/lib/common/lptimTick_u5.c
    ${SRC_DIR}/lib/common/reset_reason.c
    ${SRC_DIR}/lib/common/serial.c
    ${SRC_DIR}/lib/common/serial_console_u5.cpp
    ${SRC_DIR}/lib/common/util.c
    ${SRC_DIR}/lib/debug/debug.c
    ${SRC_DIR}/lib/debug/debug_memfault.c
    ${SRC_DIR}/lib/debug/debug_sys.c
    ${SRC_DIR}/lib/drivers/protected/protected_i2c.c
    ${SRC_DIR}/lib/drivers/protected/protected_spi.c
    ${SRC_DIR}/lib/drivers/stm32_io.c
    # ${SRC_DIR}/lib/memfault/memfault_lptim_software_watchdog.c
    ${SRC_DIR}/lib/memfault/memfault_platform_core_u5.c
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/FreeRTOS_CLI.c
    ${SRC_DIR}/third_party/printf/printf.c
    )

set(LIB_INCLUDES
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/cli
    ${SRC_DIR}/lib/debug
    ${SRC_DIR}/lib/drivers
    ${SRC_DIR}/lib/drivers/protected
    ${SRC_DIR}/lib/memfault
    ${SRC_DIR}/third_party/printf/
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/
    )

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${LIB_FILES}
    ${FREERTOS_FILES}
    ${MEMFAULT_SOURCES}
    PARENT_SCOPE)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIB_INCLUDES}
    ${FREERTOS_INCLUDES}
    ${MEMFAULT_INCLUDES}
    PARENT_SCOPE)

set(APP_DEFINES

    )

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
