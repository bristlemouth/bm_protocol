set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# Include FreeRTOS
add_subdirectory(${SRC_DIR}/third_party/FreeRTOS freertos_build)

# Include TinyUSB
add_subdirectory(${SRC_DIR}/lib/usb usb_build)


# Include memfault
include(${SRC_DIR}/third_party/memfault.cmake)

# Include BCMP
add_subdirectory(${SRC_DIR}/lib/bcmp bcmp)

# Include BM_NCP files
add_subdirectory(${SRC_DIR}/lib/bm_ncp bm_ncp)

# Include BM Common Messages
add_subdirectory(${SRC_DIR}/lib/bm_common_messages bm_common_messages)

set(BRISTLEMOUTH_FILES
    ${SRC_DIR}/lib/middleware/bm_pubsub.cpp
    ${SRC_DIR}/lib/middleware/middleware.cpp
    ${SRC_DIR}/lib/middleware/bm_network.cpp

    ${BCMP_FILES}
    ${BM_NCP_FILES}
    )

set(BRISTLEMOUTH_INCLUDES
    ${SRC_DIR}/lib/lwip
    ${SRC_DIR}/lib/middleware
    ${SRC_DIR}/third_party/
    ${SRC_DIR}/third_party/aligned_malloc
    ${BCMP_INCLUDES}
    ${BM_NCP_INCLUDES}
    ${BM_COMMON_MESSAGES_INCLUDES}
    )

set(LIB_FILES
    ${SRC_DIR}/lib/cli/cli.c
    ${SRC_DIR}/lib/common/bootloader_helper.c
    ${SRC_DIR}/lib/common/device_info.c
    ${SRC_DIR}/lib/common/enumToStr.c
    ${SRC_DIR}/lib/common/freertos_support.c
    ${SRC_DIR}/lib/common/gpioISR.c
    ${SRC_DIR}/lib/common/lib_state_machine.cpp
    ${SRC_DIR}/lib/common/lpm_u5.c
    ${SRC_DIR}/lib/common/lptimTick_u5.c
    ${SRC_DIR}/lib/common/nvmPartition.cpp
    ${SRC_DIR}/lib/common/pcap.c
    ${SRC_DIR}/lib/common/reset_reason.c
    ${SRC_DIR}/lib/common/serial.c
    ${SRC_DIR}/lib/common/serial_console_u5.cpp
    ${SRC_DIR}/lib/common/stress.c
    ${SRC_DIR}/lib/common/uptime.c
    ${SRC_DIR}/lib/bridge/bridgePowerController.cpp
    ${SRC_DIR}/lib/common/topology_sampler.cpp
    ${SRC_DIR}/lib/common/util.c
    ${SRC_DIR}/lib/common/watchdog.c
    ${SRC_DIR}/lib/common/timer_callback_handler.cpp
    ${SRC_DIR}/lib/sys/ram_partitions.c
    ${SRC_DIR}/lib/sys/configuration.cpp
    ${SRC_DIR}/lib/debug/debug.c
    ${SRC_DIR}/lib/debug/debug_bm.cpp
    ${SRC_DIR}/lib/debug/debug_gpio.c
    ${SRC_DIR}/lib/debug/debug_memfault.c
    ${SRC_DIR}/lib/debug/debug_rtc.c
    ${SRC_DIR}/lib/debug/debug_sys.cpp
    ${SRC_DIR}/lib/debug/debug_w25.cpp
    ${SRC_DIR}/lib/debug/debug_nvm_cli.cpp
    ${SRC_DIR}/lib/debug/debug_dfu.cpp
    ${SRC_DIR}/lib/debug/debug_configuration.cpp
    ${SRC_DIR}/lib/debug/debug_bridge_power_controller.cpp

    ${SRC_DIR}/lib/drivers/abstract/abstract_spi.cpp
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_bsp.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_hal.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_mac.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_phy.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_spi_oa.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adin2111.c
    ${SRC_DIR}/lib/drivers/adin2111/src/eth_adin2111.cpp
    ${SRC_DIR}/lib/drivers/pca9535.c
    ${SRC_DIR}/lib/drivers/protected/protected_i2c.c
    ${SRC_DIR}/lib/drivers/protected/protected_spi.c
    ${SRC_DIR}/lib/drivers/stm32_adc.c
    ${SRC_DIR}/lib/drivers/stm32_io.c
    ${SRC_DIR}/lib/drivers/stm32_rtc.c
    ${SRC_DIR}/lib/drivers/w25.cpp
    ${SRC_DIR}/lib/lwip/lwip_support.c
    ${SRC_DIR}/lib/memfault/memfault_platform_core_u5.c
    ${SRC_DIR}/third_party/aligned_malloc/aligned_malloc.c
    ${SRC_DIR}/third_party/cobs-c/cobs.c
    ${SRC_DIR}/third_party/crc/crc16.c
    ${SRC_DIR}/third_party/crc/crc32.c
    ${SRC_DIR}/third_party/fnv/hash_32a.c
    ${SRC_DIR}/third_party/fnv/hash_64a.c
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/FreeRTOS_CLI.c
    ${SRC_DIR}/third_party/mbedtls_base64/base64.c
    ${SRC_DIR}/third_party/printf/printf.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder_float.c
    ${SRC_DIR}/third_party/tinycbor/src/cborerrorstrings.c
    ${SRC_DIR}/third_party/tinycbor/src/cborparser.c
    ${SRC_DIR}/third_party/tinycbor/src/cborvalidation.c
    )

set(LIB_INCLUDES
    ${SRC_DIR}/lib/bcmp
    ${SRC_DIR}/lib/bridge
    ${SRC_DIR}/lib/cli
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/debug
    ${SRC_DIR}/lib/drivers
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/drivers/adin2111/include
    ${SRC_DIR}/lib/drivers/include
    ${SRC_DIR}/lib/drivers/protected
    ${SRC_DIR}/lib/lwip
    ${SRC_DIR}/lib/memfault
    ${SRC_DIR}/lib/middleware
    ${SRC_DIR}/lib/sys/
    ${SRC_DIR}/third_party/
    ${SRC_DIR}/third_party/aligned_malloc/
    ${SRC_DIR}/third_party/cobs-c
    ${SRC_DIR}/third_party/crc
    ${SRC_DIR}/third_party/fnv/
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/
    ${SRC_DIR}/third_party/printf/
    ${SRC_DIR}/third_party/tinycbor/src
    )

#
# MCUBoot files/includes
#
set(MCUBOOT_DIR ${SRC_DIR}/third_party/mcuboot)
set(MCUBOOT_FILES
    ${MCUBOOT_DIR}/boot/bootutil/src/boot_record.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_misc.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/caps.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/encrypted.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/image_ec.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/image_ec256.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/image_ed25519.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/image_rsa.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/image_validate.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/loader.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/swap_misc.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/swap_move.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/swap_scratch.c
    # ${MCUBOOT_DIR}/boot/bootutil/src/tlv.c

    # ${MCUBOOT_DIR}/ext/fiat/src/curve25519.c

    ${SRC_DIR}/lib/drivers/stm32_flash_u5.c

    ${SRC_DIR}/lib/mcuboot/port_flash.c
    ${SRC_DIR}/lib/mcuboot/port_misc.c
    )
set(MCUBOOT_INCLUDES
    ${MCUBOOT_DIR}/boot
    ${MCUBOOT_DIR}/boot/bootutil/include
    ${MCUBOOT_DIR}/boot/boot_serial/include

    # FreeRTOS port
    ${SRC_DIR}/lib/mcuboot/include

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
    # ${LWIP_DIR}/contrib
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set lwip specific defines and add to project
set(LWIP_COMPILER_FLAGS ${COMPILE_FLAGS})
include(${LWIP_DIR}/src/Filelists.cmake)

# Set bm_serial specific defines and add to project
set(BM_SERIAL_COMPILER_FLAGS ${COMPILE_FLAGS})
include(${SRC_DIR}/lib/bm_serial/CMakeLists.txt)

# Add libraries to project
set(APP_LIBS
    # I hate this, but if bm_serial goes after lwipcore
    # we get some hardfaults on the lwip library and I don't know why😭
    bm_serial
    lwipcore
    )

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external_flash_partitions.c
    ${MCUBOOT_FILES}
    ${FREERTOS_FILES}
    ${LIB_FILES}
    ${BRISTLEMOUTH_FILES}
    ${LWIP_DIR}/contrib/ports/freertos/sys_arch.c
    ${MEMFAULT_SOURCES}
    ${TINYUSB_FILES}
    ${BM_NCP_FILES}
    )

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FREERTOS_INCLUDES}
    ${LIB_INCLUDES}
    ${LWIP_INCLUDE_DIRS}
    ${BRISTLEMOUTH_INCLUDES}
    ${MCUBOOT_INCLUDES}
    ${MEMFAULT_INCLUDES}
    ${TINYUSB_INCLUDES}
    ${BM_NCP_INCLUDES}
    ${BM_SERIAL_INCLUDES}
    )

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    CFG_TUSB_MCU=OPT_MCU_STM32U5

    # Add network stress test functions
    STRESS_TEST_ENABLE
    )

if (DFU_HOST  STREQUAL 1)
list(APPEND APP_DEFINES "BM_DFU_HOST=1")
message(STATUS "DFU HOST AND CLIENT")
else()
list(APPEND APP_DEFINES "BM_DFU_HOST=0")
message(STATUS "DFU CLIENT ONLY")
endif()

if (BRIDGE_AUTO_ENABLE STREQUAL 1)
list(APPEND APP_DEFINES "BRIDGE_AUTO_ENABLE")
message(STATUS "AUTO ENABLE BRIDGE 24V!")
endif()


if (USE_MICROPYTHON STREQUAL 1)
include(${SRC_DIR}/lib/micropython/CMakeLists.txt)

list(APPEND APP_DEFINES "USE_MICROPYTHON")
list(APPEND APP_DEFINES "MICROPY_HEAP_SIZE=1024*64;${MICROPY_DEFINITIONS}")
list(APPEND APP_LIBS micropython)
# list(APPEND APP_LIBS usermod)
list(APPEND APP_INCLUDES "${MICROPY_INCLUDE_DIRS}")
list(APPEND APP_FILES
        ${SRC_DIR}/lib/micropython/micropython_freertos.c
        ${SRC_DIR}/lib/micropython/mphalport.c)

set_source_files_properties(
    ${SRC_DIR}/lib/micropython/mphalport.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS -Wno-unused-parameter
    )

set_source_files_properties(
    ${SRC_DIR}/lib/micropython/micropython_freertos.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS -Wno-unused-parameter
    )

endif()


if (NOT ${USE_BOOTLOADER})
     message(SEND_ERROR "USE_BOOTLOADER is not set.")
endif()

# Send APP_* defines to parent scope (otherwise we can't append to the lists above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
