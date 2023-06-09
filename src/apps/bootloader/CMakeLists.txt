set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

#
# MCUBoot files/includes
#
set(MCUBOOT_DIR ${SRC_DIR}/third_party/mcuboot)
set(MCUBOOT_FILES
    ${MCUBOOT_DIR}/boot/bootutil/src/boot_record.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_misc.c
    ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c
    ${MCUBOOT_DIR}/boot/bootutil/src/caps.c
    ${MCUBOOT_DIR}/boot/bootutil/src/encrypted.c
    ${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening.c
    ${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c
    ${MCUBOOT_DIR}/boot/bootutil/src/image_ec.c
    ${MCUBOOT_DIR}/boot/bootutil/src/image_ec256.c
    ${MCUBOOT_DIR}/boot/bootutil/src/image_ed25519.c
    ${MCUBOOT_DIR}/boot/bootutil/src/image_rsa.c
    ${MCUBOOT_DIR}/boot/bootutil/src/image_validate.c
    ${MCUBOOT_DIR}/boot/bootutil/src/loader.c
    ${MCUBOOT_DIR}/boot/bootutil/src/swap_misc.c
    ${MCUBOOT_DIR}/boot/bootutil/src/swap_move.c
    ${MCUBOOT_DIR}/boot/bootutil/src/swap_scratch.c
    ${MCUBOOT_DIR}/boot/bootutil/src/tlv.c

    ${MCUBOOT_DIR}/ext/fiat/src/curve25519.c

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

#
# tinycrypt files/includes
#
set(TINYCRYPT_FILES
    ${MCUBOOT_DIR}/ext/tinycrypt/lib/source/utils.c
    ${MCUBOOT_DIR}/ext/tinycrypt/lib/source/sha256.c

    ${MCUBOOT_DIR}/ext/tinycrypt-sha512/lib/source/sha512.c

    ${MCUBOOT_DIR}/ext/tinycrypt/lib/source/hmac.c
    ${MCUBOOT_DIR}/ext/tinycrypt/lib/source/aes_encrypt.c
    ${MCUBOOT_DIR}/ext/tinycrypt/lib/source/ctr_mode.c
    )
set(TINYCRYPT_INCLUDES
    ${MCUBOOT_DIR}/ext/tinycrypt/lib/include
    ${MCUBOOT_DIR}/ext/tinycrypt-sha512/lib/include
    )

#
# MBEDTLS files/includes
#
set(MBEDTLS_DIR ${MCUBOOT_DIR}/ext/mbedtls-asn1)
set(MBEDTLS_FILES
    ${MBEDTLS_DIR}/src/asn1parse.c
    ${MBEDTLS_DIR}/src/platform_util.c
    )
set(MBEDTLS_INCLUDES
    ${MBEDTLS_DIR}/include
    )

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/keys.c
    ${SRC_DIR}/lib/common/bootloader_helper.c
    ${SRC_DIR}/third_party/printf/printf.c
    ${MCUBOOT_FILES}
    ${TINYCRYPT_FILES}
    ${MBEDTLS_FILES}
    PARENT_SCOPE)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MCUBOOT_INCLUDES}
    ${TINYCRYPT_INCLUDES}
    ${MBEDTLS_INCLUDES}
    ${SRC_DIR}/lib/common
    ${SRC_DIR}/third_party/printf
    PARENT_SCOPE)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    IS_BOOTLOADER

    # Uncomment these for MCUBoot logging over SWO
    # MCUBOOT_LOG_ENABLE
    # MCUBOOT_LOG_LEVEL=MCUBOOT_LOG_LEVEL_INFO
    )

if(SIGN_IMAGES STREQUAL 1)
list(APPEND APP_DEFINES CONFIG_BOOT_SIGN_ED25519)
endif()

if(ENCRYPT_IMAGES STREQUAL 1)
list(APPEND APP_DEFINES CONFIG_BOOT_ENCRYPT_X25519)
endif()

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
