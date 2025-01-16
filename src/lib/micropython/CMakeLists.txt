cmake_minimum_required(VERSION 3.20.0)

# project(micropython)


set(MICROPY_PORT_DIR    ${SRC_DIR}/lib/micropython)
set(MICROPY_DIR         ${SRC_DIR}/third_party/micropython)
set(MICROPY_LIB_DIR     ${SRC_DIR}/third_party/micropython-lib)
set(MICROPY_TARGET      micropython)

set(USER_C_MODULES ${MICROPY_PORT_DIR}/modules/micropython.cmake)
include(${MICROPY_DIR}/py/usermod.cmake)

include(${MICROPY_DIR}/py/py.cmake)
include(${MICROPY_DIR}/extmod/extmod.cmake)

set(MICROPY_SOURCE_PORT
    # main.c
    # help.c
    modules/machine_i2c.c
    # machine_spi.c
    # machine_pin.c
    # machine_uart.c
    # modbluetooth_zephyr.c
    modules/modmachine.c
    # modusocket.c
    modules/modutime.c
    # modzephyr.c
    # modzsensor.c
    # mphalport.c
    # uart_core.c
    # zephyr_storage.c
)
list(TRANSFORM MICROPY_SOURCE_PORT PREPEND ${MICROPY_PORT_DIR}/)

set(MICROPY_SOURCE_SHARED
    # libc/printf.c
    readline/readline.c
    # runtime/interrupt_char.c
    runtime/gchelper_generic.c
    # runtime/mpirq.c
    runtime/pyexec.c
    runtime/stdout_helpers.c
    timeutils/timeutils.c
)
list(TRANSFORM MICROPY_SOURCE_SHARED PREPEND ${MICROPY_DIR}/shared/)

set(MICROPY_SOURCE_LIB
    # oofatfs/ff.c
    # oofatfs/ffunicode.c
    # littlefs/lfs1.c
    # littlefs/lfs1_util.c
    # littlefs/lfs2.c
    # littlefs/lfs2_util.c
)
list(TRANSFORM MICROPY_SOURCE_LIB PREPEND ${MICROPY_DIR}/lib/)

set(MICROPY_SOURCE_QSTR
    ${MICROPY_SOURCE_PY}
    ${MICROPY_SOURCE_EXTMOD}
    ${MICROPY_SOURCE_USERMOD}
    ${MICROPY_SOURCE_SHARED}
    ${MICROPY_SOURCE_LIB}
    ${MICROPY_SOURCE_PORT}
)

set(MICROPY_ONLY_INCLUDE_DIRS
    ${MICROPY_INC_CORE}
    ${MICROPY_PORT_DIR}
    ${CMAKE_BINARY_DIR}
    )

set(MICROPY_INCLUDE_DIRS
    ${MICROPY_ONLY_INCLUDE_DIRS}
    ${APP_INCLUDES}
    ${BSP_INCLUDES}
    ${LIB_INCLUDES}
    ${THIRD_PARTY_INCLUDES}
    )

message("APP DEFINES ${APP_DEFINES}")

set(MICROPY_DEFINITIONS
    MP_CONFIGFILE="mpconfigport.h"
    ${BSP_DEFINES}
    ${APP_DEFINES}
    )

set(MICROPY_CROSS_FLAGS -march=armv7m)
set(MICROPY_FROZEN_MANIFEST ${MICROPY_PORT_DIR}/manifest.py)

add_library(micropython EXCLUDE_FROM_ALL ${MICROPY_SOURCE_QSTR})
target_compile_options(micropython PRIVATE ${MICROPY_COMPILER_FLAGS})
target_compile_definitions(micropython PRIVATE ${MICROPY_DEFINITIONS})
target_include_directories(micropython PRIVATE ${MICROPY_INCLUDE_DIRS})

# Make sure `make clean` removes genhdr directory. Otherwise re-building might fail
set_target_properties(micropython PROPERTIES ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/genhdr;${CMAKE_BINARY_DIR}/frozen_mpy")

include(${MICROPY_DIR}/py/mkrules.cmake)

set(MICROPY_INCLUDE_DIRS ${MICROPY_ONLY_INCLUDE_DIRS} PARENT_SCOPE)
