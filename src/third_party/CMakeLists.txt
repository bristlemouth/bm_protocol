#
# MCUBoot files/includes
#
set(MCUBOOT_DIR ${SRC_DIR}/third_party/mcuboot)
set(MCUBOOT_FILES
    ${MCUBOOT_DIR}/boot/bootutil/src/boot_record.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_misc.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c
    ${SRC_DIR}/lib/drivers/stm32_flash_u5.c
    ${SRC_DIR}/lib/mcuboot/port_flash.c
    ${SRC_DIR}/lib/mcuboot/port_misc.c
    # uncomment for the `update sec`, `update clr`, `update confirm` commands
    # ${SRC_DIR}/lib/mcuboot/mcuboot_cli.c
    )
set(MCUBOOT_INCLUDES
    ${MCUBOOT_DIR}/boot
    ${MCUBOOT_DIR}/boot/bootutil/include
    ${MCUBOOT_DIR}/boot/boot_serial/include

    # FreeRTOS port
    ${SRC_DIR}/lib/mcuboot/include
    # uncomment for the `update sec`, `update clr`, `update confirm` commands
    # ${SRC_DIR}/lib/mcuboot/
    ${SRC_DIR}/lib/drivers/
    )

set_source_files_properties(
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_misc.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c
    ${MCUBOOT_DIR}/boot/bootutil/src/swap_misc.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS -Wno-unused-parameter
    )

set_source_files_properties(
    ${SRC_DIR}/third_party/mcuboot/boot/bootutil/src/swap_scratch.c
    ${MCUBOOT_DIR}/ext/fiat/src/curve25519.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS -Wno-unused-but-set-variable
    )

set_source_files_properties(
    ${SRC_DIR}/third_party/mcuboot/boot/bootutil/src/loader.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS "-Wno-unused-parameter -Wno-format -Wno-unused-but-set-variable"
    )



set(LWIP_DIR ${SRC_DIR}/third_party/lwip)
set(LWIP_INCLUDE_DIRS
    ${LWIP_DIR}/src/include
    ${LWIP_DIR}/contrib/ports/freertos/include
    ${SRC_DIR}/lib/lwip
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Include FreeRTOS
add_subdirectory(${SRC_DIR}/third_party/FreeRTOS freertos_build)

# Include memfault
include(${SRC_DIR}/third_party/memfault.cmake)

# Set lwip specific defines and add to project
set(LWIP_COMPILER_FLAGS ${COMPILE_FLAGS})
include(${LWIP_DIR}/src/Filelists.cmake)

set(THIRD_PARTY_FILES
    ${LWIP_DIR}/contrib/ports/freertos/sys_arch.c
    ${SRC_DIR}/third_party/BME280_driver/bme280.c
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/FreeRTOS_CLI.c
    ${SRC_DIR}/third_party/fnv/hash_32a.c
    ${SRC_DIR}/third_party/fnv/hash_64a.c
    ${SRC_DIR}/third_party/printf/printf.c
    ${SRC_DIR}/third_party/mbedtls_base64/base64.c
    ${SRC_DIR}/third_party/cobs-c/cobs.c
    ${FREERTOS_FILES}
    ${MEMFAULT_SOURCES}
    ${MCUBOOT_FILES}
    PARENT_SCOPE
)

set(THIRD_PARTY_INCLUDES
    ${SRC_DIR}/third_party/BME280_driver
    ${SRC_DIR}/third_party/fnv
    ${SRC_DIR}/third_party/mbedtls_base64
    ${SRC_DIR}/third_party/cobs-c
    ${LWIP_INCLUDE_DIRS}
    ${FREERTOS_INCLUDES}
    ${MEMFAULT_INCLUDES}
    ${MCUBOOT_INCLUDES}
    PARENT_SCOPE
)

# Add libraries to project
set(THIRD_PARTY_LIBS
    # I hate this, but if bm_serial goes after lwipcore
    # we get some hardfaults on the lwip library and I don't know why😭
    lwipcore
)
