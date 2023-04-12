set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# Include FreeRTOS
add_subdirectory(${SRC_DIR}/third_party/FreeRTOS freertos_build)

# Include TinyUSB
add_subdirectory(${SRC_DIR}/lib/usb usb_build)

# Include memfault
include(${SRC_DIR}/third_party/memfault.cmake)

set(LIB_FILES
    ${SRC_DIR}/lib/bcl/src/bm_config.c
    ${SRC_DIR}/lib/bcl/src/bm_info.c
    ${SRC_DIR}/lib/bcl/src/bm_l2.c
    ${SRC_DIR}/lib/bcl/src/bm_network.c
    ${SRC_DIR}/lib/bcl/src/bm_util.c
    ${SRC_DIR}/lib/bcl/src/bm_zcbor_decode.c
    ${SRC_DIR}/lib/bcl/src/bm_zcbor_encode.c
    ${SRC_DIR}/lib/bcl/src/bristlemouth.c
    ${SRC_DIR}/lib/cli/cli.c
    ${SRC_DIR}/lib/common/bootloader_helper.c
    ${SRC_DIR}/lib/common/device_info.c
    ${SRC_DIR}/lib/common/enumToStr.c
    ${SRC_DIR}/lib/common/freertos_cpp_overrides.cpp
    ${SRC_DIR}/lib/common/freertos_support.c
    ${SRC_DIR}/lib/common/gpioISR.c
    ${SRC_DIR}/lib/common/lpm_u5.c
    ${SRC_DIR}/lib/common/lptimTick_u5.c
    ${SRC_DIR}/lib/common/pcap.c
    ${SRC_DIR}/lib/common/reset_reason.c
    ${SRC_DIR}/lib/common/serial.c
    ${SRC_DIR}/lib/common/serial_console_u5.cpp
    ${SRC_DIR}/lib/common/util.c
    ${SRC_DIR}/lib/common/watchdog.c
    ${SRC_DIR}/lib/common/nvmPartition.cpp
    ${SRC_DIR}/lib/sys/external_flash_partitions.c
    ${SRC_DIR}/lib/sys/ram_partitions.c
    ${SRC_DIR}/lib/sys/configuration.cpp
    ${SRC_DIR}/lib/debug/debug.c
    ${SRC_DIR}/lib/debug/debug_adin_raw.c
    ${SRC_DIR}/lib/debug/debug_htu.cpp
    ${SRC_DIR}/lib/debug/debug_bm.c
    ${SRC_DIR}/lib/debug/debug_gpio.c
    ${SRC_DIR}/lib/debug/debug_memfault.c
    # ${SRC_DIR}/lib/debug/debug_mic.cpp
    ${SRC_DIR}/lib/debug/debug_i2c.c
    ${SRC_DIR}/lib/debug/debug_ina232.cpp
    ${SRC_DIR}/lib/debug/debug_spi.c
    ${SRC_DIR}/lib/debug/debug_sys.c
    ${SRC_DIR}/lib/debug/debug_uart.c
    ${SRC_DIR}/lib/debug/debug_w25.cpp
    ${SRC_DIR}/lib/debug/debug_configuration.cpp
    ${SRC_DIR}/lib/dfu/src/bm_dfu_client.c
    ${SRC_DIR}/lib/dfu/src/bm_dfu_core.cpp
    ${SRC_DIR}/lib/dfu/src/bm_dfu_host.c
    ${SRC_DIR}/lib/drivers/abstract/abstract_i2c.cpp
    ${SRC_DIR}/lib/drivers/abstract/abstract_spi.cpp
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_bsp.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_hal.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_mac.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_phy.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adi_spi_oa.c
    ${SRC_DIR}/lib/drivers/adin2111/src/adin2111.c
    ${SRC_DIR}/lib/drivers/adin2111/src/eth_adin2111.c
    ${SRC_DIR}/lib/drivers/htu21d.cpp
    ${SRC_DIR}/lib/drivers/ina232.cpp
    # ${SRC_DIR}/lib/drivers/mic.cpp
    ${SRC_DIR}/lib/drivers/pca9535.c
    ${SRC_DIR}/lib/drivers/w25.cpp
    ${SRC_DIR}/lib/drivers/protected/protected_i2c.c
    ${SRC_DIR}/lib/drivers/protected/protected_spi.c
    ${SRC_DIR}/lib/drivers/stm32_adc.c
    ${SRC_DIR}/lib/drivers/stm32_io.c
    ${SRC_DIR}/lib/lwip/lwip_support.c
    ${SRC_DIR}/lib/memfault/memfault_platform_core_u5.c
    ${SRC_DIR}/lib/middleware/bm_pubsub.c
    ${SRC_DIR}/lib/middleware/middleware.cpp
    ${SRC_DIR}/third_party/aligned_malloc/aligned_malloc.c
    ${SRC_DIR}/third_party/crc/crc32.c
    ${SRC_DIR}/third_party/fnv/hash_32a.c
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/FreeRTOS_CLI.c
    ${SRC_DIR}/third_party/mbedtls_base64/base64.c
    ${SRC_DIR}/third_party/printf/printf.c
    ${SRC_DIR}/third_party/zcbor/src/zcbor_common.c
    ${SRC_DIR}/third_party/zcbor/src/zcbor_decode.c
    ${SRC_DIR}/third_party/zcbor/src/zcbor_encode.c
    ${SRC_DIR}/third_party/tinycbor/src/cborparser.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder_float.c
    ${SRC_DIR}/third_party/tinycbor/src/cborencoder.c
    ${SRC_DIR}/third_party/tinycbor/src/cborerrorstrings.c
    ${SRC_DIR}/third_party/tinycbor/src/cborvalidation.c
    )

set(LIB_INCLUDES
    ${SRC_DIR}/lib/bcl/include
    ${SRC_DIR}/lib/cli
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/lib/debug
    ${SRC_DIR}/lib/dfu/include
    ${SRC_DIR}/lib/drivers
    ${SRC_DIR}/lib/sys
    ${SRC_DIR}/lib/drivers/abstract
    ${SRC_DIR}/lib/drivers/adin2111/include
    ${SRC_DIR}/lib/drivers/include
    ${SRC_DIR}/lib/drivers/protected
    ${SRC_DIR}/lib/lwip
    ${SRC_DIR}/lib/mcuboot
    ${SRC_DIR}/lib/memfault
    ${SRC_DIR}/lib/middleware
    ${SRC_DIR}/third_party/
    ${SRC_DIR}/third_party/aligned_malloc
    ${SRC_DIR}/third_party/crc
    ${SRC_DIR}/third_party/fnv/
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/
    ${SRC_DIR}/third_party/printf/
    ${SRC_DIR}/third_party/zcbor/include
    ${SRC_DIR}/third_party/tinycbor/src
    )

set_source_files_properties(
    ${SRC_DIR}/lib/bcl/src/bm_zcbor_decode.c
    ${SRC_DIR}/lib/bcl/src/bm_zcbor_encode.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS -Wno-type-limits
    )

set_source_files_properties(
    ${SRC_DIR}/third_party/zcbor/src/zcbor_decode.c
    ${SRC_DIR}/third_party/zcbor/src/zcbor_encode.c
    ${SRC_DIR}/third_party/zcbor/src/zcbor_common.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare"
    )

if (USE_BOOTLOADER STREQUAL 1)
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

    ${SRC_DIR}/lib/mcuboot/mcuboot_cli.c
    ${SRC_DIR}/lib/mcuboot/port_flash.c
    ${SRC_DIR}/lib/mcuboot/port_misc.c
    )
set(MCUBOOT_INCLUDES
    ${MCUBOOT_DIR}/boot
    ${MCUBOOT_DIR}/boot/bootutil/include
    ${MCUBOOT_DIR}/boot/boot_serial/include

    # FreeRTOS port
    ${SRC_DIR}/lib/mcuboot/
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
endif() # USE_BOOTLOADER = 1

set(LWIP_DIR ${SRC_DIR}/third_party/lwip)
set(LWIP_INCLUDE_DIRS
    ${LWIP_DIR}/src/include
    ${LWIP_DIR}/contrib/ports/freertos/include
    ${SRC_DIR}/lib/lwip
    # ${LWIP_DIR}/contrib
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include(${LWIP_DIR}/src/Filelists.cmake)
# include(${LWIP_DIR}/contrib/Filelists.cmake)

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${LIB_FILES}
    ${FREERTOS_FILES}
    ${MEMFAULT_SOURCES}
    ${LWIP_DIR}/contrib/ports/freertos/sys_arch.c
    ${MCUBOOT_FILES}
    ${TINYUSB_FILES}
    PARENT_SCOPE)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIB_INCLUDES}
    ${FREERTOS_INCLUDES}
    ${MEMFAULT_INCLUDES}
    ${MCUBOOT_INCLUDES}
    ${LWIP_INCLUDE_DIRS}
    ${TINYUSB_INCLUDES}
    PARENT_SCOPE)

set(APP_LIBS
    lwipcore
    PARENT_SCOPE)

set(APP_DEFINES
    CFG_TUSB_MCU=OPT_MCU_STM32U5
    # NO_NETWORK
    )

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
