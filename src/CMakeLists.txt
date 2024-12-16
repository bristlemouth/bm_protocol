# If we are on windows (CMAKE uses win32 for both win32 and win64), we need to set the python executable to python
# even though we are using python3.
if(CMAKE_HOST_WIN32)
	set(PYTHON "python")
else()
	set(PYTHON "python3")
endif()

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Change these whenever updating compiler in environment.yml
set(COMPILER_ID "GNU")
set(COMPILER_VERSION "13.2.1")

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL COMPILER_ID OR NOT CMAKE_C_COMPILER_ID STREQUAL COMPILER_ID)
	message(SEND_ERROR "Should be using ${COMPILER_ID}, not ${CMAKE_CXX_COMPILER_ID}!")
	return()
endif()

if(NOT CMAKE_CXX_COMPILER_VERSION STREQUAL COMPILER_VERSION OR NOT CMAKE_C_COMPILER_VERSION STREQUAL COMPILER_VERSION)
	message(SEND_ERROR "Should be using GCC ${COMPILER_VERSION}, not ${CMAKE_C_COMPILER_VERSION}! Update your conda environment and try again.")
	return()
endif()

if (NOT DEFINED BSP)
	message(SEND_ERROR "BSP is not defined")
	return()
endif()

if (NOT DEFINED APP)
	message(SEND_ERROR "APP is not defined")
	return()
endif()

# Default to USE_BOOTLOADER unless otherwise set
if (NOT USE_BOOTLOADER STREQUAL 0 AND NOT APP STREQUAL "bootloader")
set(USE_BOOTLOADER 1)
endif()

# Make sure we don't try to use MCUBoot signing/etc with the actual bootloader app
if (APP STREQUAL "bootloader" AND USE_BOOTLOADER STREQUAL 1)
	message(SEND_ERROR "Cannot build bootloader app with USE_BOOTLOADER defined!")
	return()
endif()

if (USE_BOOTLOADER STREQUAL 1)
	set(BOOTLOADER_DEFINES USE_BOOTLOADER)
else()
	message(STATUS "Using NO BOOTLOADER configuration")
endif()

# Set a default build type if none was specified
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build."
      FORCE
  )

  set_property(CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
  )
endif()

set(SCRIPTS_PATH ${CMAKE_SOURCE_DIR}/tools/scripts)

# Path where custom openocd .cfg files and other debug goodies are located
set(DEBUG_SCRIPTS_PATH ${SCRIPTS_PATH}/debug)

#
# Linker variables below. Modifiy as necessary here or override
# in app/bsp CMakeLists.txt files
#
# Flash start address for STM32U575
set(FLASH_START 0x8000000)

# No Bootloader by default
set(BOOTLOADER_SIZE 0)

# 1276kB Application size (2048kB-772kB for coredump)
# TODO - change this when we use 1MB flash part
set(APP_SIZE 0x13F000)

# No MCUBoot header by default
set(MCUBOOT_HEADER_SIZE 0)

# Default to application flash
set(LINKER_FLASH APP_FLASH)

# Use the correct linker settings when using bootloader mode
# (or compiling bootloader)
if (USE_BOOTLOADER STREQUAL 1 OR APP STREQUAL "bootloader")
#
#   Memory Map when bootloader is in use
#
# 		+----------------------+ = FLASH_START
# 		|                      |
# 		|      bootloader      |
# 		|                      |
# 		|----------------------| + BOOTLOADER_SIZE
# 		|    Image 1 Header    |
# 		|----------------------| + MCUBOOT_HEADER_SIZE
# 		|                      |
# 		|       Image 1        |
# 		|                      |
# 		-----------------------| + APP_SIZE
# 		|    Image 2 Header    |
# 		|----------------------| + MCUBOOT_HEADER_SIZE
# 		|                      |
# 		|       Image 2        |
# 		|                      |
# 		|----------------------| + APP_SIZE
# 		|     Scratch Area     |
# 		+----------------------+

# 48kB Bootloader
set(BOOTLOADER_SIZE 0xC000)

# 968kB Application size, since we have two slots
# (and want to align to 8kB boundaries for flash erase)
set(APP_SIZE 0xF2000)

# 64kB scratch size
set(MCUBOOT_SCRATCH_SIZE 0x10000)

# MCUBOOT can only write flash 16 bytes at a time on the STM32U5
set(MCUBOOT_BOOT_MAX_ALIGN 16)

# Figure out which flash section we're going to use
if (APP STREQUAL "bootloader")
	set(LINKER_FLASH BOOT_FLASH)
	set(MCUBOOT_HEADER_SIZE 0)
else()
    message(STATUS "Using BOOTLOADER configuration")
    set(LINKER_FLASH APP_FLASH)
	set(MCUBOOT_HEADER_SIZE 0x200)
endif()
endif() # USE_BOOTLOADER STREQUAL 1 OR APP STREQUAL "bootloader"

set(HW_VERSION 0)

# Include BSP and application CMakeLists.txt files
# They _should_ be able to override any of the above variables
add_subdirectory(bsp/${BSP})

# This MUST be after the BSP include to get ${BSP_CPU_FLAGS}
set(COMPILE_FLAGS
	-g
	-ggdb

	# Necessary to prevent newlib's printf from stomping on our definition
	-fno-builtin-printf

	$<$<CONFIG:Debug>:-Og>
	$<$<CONFIG:Release>:-O2>

	-ffile-prefix-map=${SRC_DIR}=.
	-fdebug-prefix-map=${SRC_DIR}=.
	-fdebug-prefix-map=${CMAKE_BINARY_DIR}=.
	-fdebug-prefix-map=$ENV{CONDA_PREFIX}=.

	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>

	-Wformat=2
	-Wformat-overflow
	-Wformat-signedness
	-Wno-format
	-Wno-narrowing

	-Werror

# In case we want faster sqrtf, etc
	# -fno-math-errno
	# -fsingle-precision-constant
	# -ffast-math

	${BSP_CPU_FLAGS}

	-DCBOR_CUSTOM_ALLOC_INCLUDE="tinycbor_alloc.h"
	-DCBOR_PARSER_MAX_RECURSIONS=10
	)

set(MICROPY_COMPILER_FLAGS
	-g
	-ggdb

	# Necessary to prevent newlib's printf from stomping on our definition
	-fno-builtin-printf

	-fomit-frame-pointer

	$<$<CONFIG:Debug>:-Og>
	$<$<CONFIG:Release>:-O2>

	-ffile-prefix-map=${SRC_DIR}=.
	-fdebug-prefix-map=${SRC_DIR}=.
	-fdebug-prefix-map=${CMAKE_BINARY_DIR}=.
	-fdebug-prefix-map=$ENV{CONDA_PREFIX}=.

	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
	$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>

	-Wno-unused-parameter

	# GRRRR
	# -Werror

# In case we want faster sqrtf, etc
	# -fno-math-errno
	# -fsingle-precision-constant
	# -ffast-math

	${BSP_CPU_FLAGS}
	)
add_compile_options(${COMPILE_FLAGS})

set(APP_NAME ${APP})

set(APP_DIR_COMMON "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(APP_NAME ${APP_NAME}-dbg)
endif()
if(CMAKE_APP_TYPE STREQUAL "BMDK")
    set(APP_DIR_COMMON ${CMAKE_CURRENT_LIST_DIR}/apps/bm_devkit/bmdk_common)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/apps/bm_devkit/${APP})
elseif(CMAKE_APP_TYPE STREQUAL "bristleback")
    set(APP_DIR_COMMON ${CMAKE_CURRENT_LIST_DIR}/apps/bristleback_apps/bristleback_apps_common)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/apps/bristleback_apps/${APP})
elseif(CMAKE_APP_TYPE STREQUAL "rs232_expander")
    set(APP_DIR_COMMON ${CMAKE_CURRENT_LIST_DIR}/apps/rs232_expander_apps/rs232_expander_apps_common)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/apps/rs232_expander_apps/${APP})
else()
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/apps/${APP})
endif()

if (DFU_HOST  STREQUAL 1)
list(APPEND APP_DEFINES "BM_DFU_HOST=1")
message(STATUS "DFU HOST AND CLIENT")
else()
list(APPEND APP_DEFINES "BM_DFU_HOST=0")
message(STATUS "DFU CLIENT ONLY")
endif()

# TODO - remove once we are using just one type of Aanderaa current meter
# Used for differentiation between Blue and 4830 Aanderaa current meters
if(${APP} STREQUAL "aanderaa")
	if(NOT DEFINED CMAKE_AANDERAA_TYPE)
		message(SEND_ERROR "CMAKE_AANDERAA_TYPE is not defined")
		return()
	elseif(CMAKE_AANDERAA_TYPE STREQUAL "BLUE")
		list(APPEND APP_DEFINES "AANDERAA_BLUE")
	elseif(CMAKE_AANDERAA_TYPE STREQUAL "4830")
		list(APPEND APP_DEFINES "AANDERAA_4830")
	elseif(CMAKE_AANDERAA_TYPE STREQUAL "FAKE_AANDERAA")
		list(APPEND APP_DEFINES "AANDERAA_4830")
		list(APPEND APP_DEFINES "FAKE_AANDERAA")
	else()
		message(SEND_ERROR "CMAKE_AANDERAA_TYPE is none of 4830, BLUE or FAKE_AANDERAA")
		return()
	endif()
endif()

set(EXECUTABLE ${BSP}-${APP_NAME}.elf)

# Compute addresses to be used later for flashing device and padding images
math(EXPR APP_FLASH_ADDR "${FLASH_START} + ${BOOTLOADER_SIZE}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR APP_IMG2_FLASH_ADDR "${FLASH_START} + ${BOOTLOADER_SIZE} + ${APP_SIZE}" OUTPUT_FORMAT HEXADECIMAL)

set(LINKER_DEFINES
    FLASH__=${LINKER_FLASH}
	FLASH_START=${FLASH_START}
	BOOTLOADER_SIZE=${BOOTLOADER_SIZE}
	APP_SIZE=${APP_SIZE}
	MCUBOOT_HEADER_SIZE=${MCUBOOT_HEADER_SIZE}
	SCRATCH_SIZE=${MCUBOOT_SCRATCH_SIZE}
 )

add_compile_options(${COMPILE_FLAGS})

# set(LINKER_FILE spotter_stm32l496)
set(LINKER_FILE bs_stm32u575)
# set(LINKER_FILE STM32U575ZITXQ_FLASH)

# Autogenerate custom linker file depending on whether we're using application or
# bootloader. This way both use the same linker script and we can guarantee that
# they will not overlap and have the same RAM sections.
#
# For more information, see:
# https://interrupt.memfault.com/blog/noinit-memory#bootloader-and-application-shared-linker-script
# https://cmake.org/pipermail/cmake/2013-November/056322.html
#
set(BOOT_LINKER_FILE ${SRC_DIR}/bsp/common/linker/${LINKER_FILE}.ld)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${LINKER_FILE}.lds
	MAIN_DEPENDENCY ${BOOT_LINKER_FILE}
	COMMAND ${CMAKE_C_COMPILER}
	-E -P -C -x c-header
	-DFLASH__=${LINKER_FLASH}
	-DFLASH_START=${FLASH_START}
	-DBOOTLOADER_SIZE=${BOOTLOADER_SIZE}
	-DAPP_SIZE=${APP_SIZE}
	-DMCUBOOT_HEADER_SIZE=${MCUBOOT_HEADER_SIZE}
	${BOOT_LINKER_FILE} > ${CMAKE_CURRENT_BINARY_DIR}/${LINKER_FILE}.lds
	VERBATIM)

add_custom_target(linkerscript
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${LINKER_FILE}.lds
	VERBATIM)

add_link_options(
        -T${CMAKE_CURRENT_BINARY_DIR}/${LINKER_FILE}.lds
        ${BSP_CPU_FLAGS}
				-Wl,--build-id
        # -specs=nano.specs
        # -lc
        # -lm
        # -lnosys
        # -Wl,-Map=${PROJECT_NAME}.map,--cref
        # -Wl,--gc-sections
        )

add_executable(${EXECUTABLE})

# Include LIB directory
if(NOT APP MATCHES "bootloader")
add_subdirectory(lib)
add_subdirectory(third_party)
endif()

add_compile_definitions(
	${BSP_DEFINES}
    ${LIB_DEFINES}
	${APP_DEFINES}
	${LINKER_DEFINES}
	${BOOTLOADER_DEFINES}
	MCUBOOT_BOOT_MAX_ALIGN=${MCUBOOT_BOOT_MAX_ALIGN}
	$<$<CONFIG:Debug>:BUILD_DEBUG=1>
	$<$<CONFIG:Release>:BUILD_DEBUG=0>
	)

# Make sure the linker file processing gets run before main app is built
add_dependencies(${EXECUTABLE} linkerscript)

# Override bootloader release optimization for size (vs speed)
if (APP STREQUAL "bootloader")
	target_compile_options(${EXECUTABLE}
		PUBLIC $<$<CONFIG:Release>:-Os>
		)
endif()

#
# Bootloader encryption/signing keys
#
set(IMGTOOL ${SRC_DIR}/third_party/mcuboot/scripts/imgtool.py)

# If ED25519_KEY_FILE passed as argument, use it, otherwise use default keyfile
string(LENGTH "${ED25519_KEY_FILE}" ED25519_KEY_FILE_LEN)
if(ED25519_KEY_FILE_LEN LESS_EQUAL 0)
# Default development key file
set(ED25519_KEY_FILE ${SRC_DIR}/apps/bootloader/ed25519_key.pem)
endif()

# If X25519_KEY_FILE passed as argument, use it, otherwise use default keyfile
string(LENGTH "${X25519_KEY_FILE}" X25519_KEY_FILE_LEN)
if(X25519_KEY_FILE_LEN LESS_EQUAL 0)
# Default development key file
set(X25519_KEY_FILE ${SRC_DIR}/apps/bootloader/x25519_key.pem)
endif()

if (APP STREQUAL "bootloader")
set(KEY_FILES "")

if(SIGN_IMAGES STREQUAL 1)
list(APPEND KEY_FILES "${CMAKE_CURRENT_BINARY_DIR}/ed25519_pub_key.c")
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ed25519_pub_key.c
	MAIN_DEPENDENCY ${ED25519_KEY_FILE}
	COMMAND ${IMGTOOL} getpub -k ${ED25519_KEY_FILE} > ${CMAKE_CURRENT_BINARY_DIR}/ed25519_pub_key.c
	VERBATIM)
endif()

if(ENCRYPT_IMAGES STREQUAL 1)
list(APPEND KEY_FILES "${CMAKE_CURRENT_BINARY_DIR}/x25519_priv_key.c")
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/x25519_priv_key.c
	MAIN_DEPENDENCY ${X25519_KEY_FILE}
	COMMAND ${IMGTOOL} getpriv -k ${X25519_KEY_FILE} > ${CMAKE_CURRENT_BINARY_DIR}/x25519_priv_key.c
	VERBATIM)
endif()

endif() # if (APP STREQUAL "bootloader")

target_sources(${EXECUTABLE} PRIVATE
	${BSP_FILES}
    ${LIB_FILES}
    ${THIRD_PARTY_FILES}
	${APP_FILES}
	${KEY_FILES}
	${CMAKE_CURRENT_BINARY_DIR}/version.c)

target_include_directories(${EXECUTABLE} PRIVATE
	${BSP_INCLUDES}
    ${LIB_INCLUDES}
    ${APP_DIR_COMMON}
    ${THIRD_PARTY_INCLUDES}
	${APP_INCLUDES}
	)

if(NOT APP MATCHES "bootloader")
# Include bm_core
set(BM_CORE_LWIP_INCLUDES
    ${LIB_DIR}/lwip
    ${THIRD_PARTY_INCLUDES}
)

set(BM_CORE_FREERTOS_INCLUDES
    ${THIRD_PARTY_INCLUDES}
    ${LIB_DIR}/common
    ${MEMFAULT_INCLUDES}
    ${APP_DIR}
    ${APP_DIR_COMMON}
)

include_directories("${LIB_DIR}/bm_integration")
include(${LIB_DIR}/bm_core/cmake/bm_core.cmake)
setup_bm_ip_stack(LWIP "${BM_CORE_LWIP_INCLUDES}")
setup_bm_os(FREERTOS "${BM_CORE_FREERTOS_INCLUDES}")
add_subdirectory(${LIB_DIR}/bm_core bmcore)
target_compile_definitions(middleware PRIVATE ${APP_DEFINES})
target_link_libraries(bmcore PUBLIC lwipcore)

if (USE_MICROPYTHON STREQUAL 1)
include(${LIB_DIR}/micropython/CMakeLists.txt)

list(APPEND APP_DEFINES "USE_MICROPYTHON")
list(APPEND APP_DEFINES "MICROPY_HEAP_SIZE=1024*64;${MICROPY_DEFINITIONS}")
list(APPEND APP_LIBS micropython)
list(APPEND APP_INCLUDES "${MICROPY_INCLUDE_DIRS}")
list(APPEND APP_FILES
        ${LIB_DIR}/micropython/micropython_freertos.c
        ${LIB_DIR}/micropython/mphalport.c)

target_compile_options(
    micropython PRIVATE
    -Wno-dangling-pointer -Wno-sign-compare
    )

set_source_files_properties(
    ${LIB_DIR}/micropython/mphalport.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS -Wno-unused-parameter
    )

set_source_files_properties(
    ${LIB_DIR}/micropython/micropython_freertos.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS -Wno-unused-parameter
    )

endif()


# Link any libraries the application might have included
target_link_libraries(${EXECUTABLE}
    bmcore
    "${APP_LIBS}"
    "${THIRD_PARTY_LIBS}"
    "${LIB_LIBS}"
)

else()

# Link any libraries the application might have included
target_link_libraries(${EXECUTABLE}
    "${APP_LIBS}"
)

endif()

target_linker_map(${EXECUTABLE})

# Print executable size
add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
        COMMAND ${SIZE} ${EXECUTABLE})

# Generate hex file
add_custom_command(
	TARGET ${EXECUTABLE} POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} -O ihex ${EXECUTABLE} ${EXECUTABLE}.hex
	BYPRODUCTS ${EXECUTABLE}.hex)

# Generate bin file
add_custom_command(
	TARGET ${EXECUTABLE} POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE} ${EXECUTABLE}.bin
	BYPRODUCTS ${EXECUTABLE}.bin
)

#
# Let me explain the horrible looking command that follows...
# Since we compiled with -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.
# the elf file only has relative paths to all the source files.
# When trying to generate the assembly listing intermixed with source code,
# objdump can't find the source files. It looks in the current directory + the
# relative path to the file. I tried using the --include= flag with no luck.
#
# The --prefix flag adds the cmake source directory, but then objdump appends
# the whole path to the current build directory AND relative file paths...
# The --prefix-strip=<n> command removes the first <n> directories from the
# file paths. This number varies depending on where the code is being built,
# and that's where `echo "$$PWD" | grep -o '/' - | wc -l | bc` comes in...
# We first print the current working directory and feed it into grep, which
# then searches for '/' and outputs each instance in a new line. `wc -l` then
# outputs the actual number of lines. Unfortunately, it also outputs a bunch
# of whitespace before the number. That's where `bc` comes in and strips the
# whitespace away. I don't know how, but it was recommended in an SO post
# here: https://stackoverflow.com/a/53177403
#
# The sed command at the end just strips the absolute path, once again,
# to make the .lst file consistent across different build paths.
#
# If you come up with a better solution, please remove this :D
#
if(CMAKE_HOST_WIN32)
	# This is the windows version of the horrible command
	set(HORRIBLE_COMMAND, "bash -c \"${CMAKE_OBJDUMP} --prefix=${SRC_DIR} --prefix-strip=$$(echo $${PWD//\\//} | tr -cd '/' | wc -c) -S -d -l -w ${EXECUTABLE} | tr -s ' ' | cut -d' ' -f2- | sed 's=${SRC_DIR}/./==' > ${EXECUTABLE}.lst\"")
else()
	set(HORRIBLE_COMMAND, "${CMAKE_OBJDUMP} --prefix=${SRC_DIR} --prefix-strip=`echo \"$$PWD\" | grep -o '/' - | wc -l | bc` -S -d -l -w ${EXECUTABLE} | sed 's=${SRC_DIR}/./==' > ${EXECUTABLE}.lst")
endif()
	# Generate lst file
add_custom_command(TARGET ${EXECUTABLE}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${EXECUTABLE} ${EXECUTABLE}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE} ${EXECUTABLE}.bin

	COMMAND ${HORRIBLE_COMMAND}
	BYPRODUCTS ${EXECUTABLE}.lst
)

if (USE_BOOTLOADER STREQUAL 1)

if(SIGN_IMAGES STREQUAL 1)
set(SIGN_IMAGE_CMD --key ${ED25519_KEY_FILE})
message(STATUS "Using ed25519 signed MCUBoot images.")
else()
message(STATUS "Using unsigned MCUBoot images.")
# Make sure we clear if value is other than 1
set(SIGN_IMAGES 0)
endif()

if(ENCRYPT_IMAGES STREQUAL 1)
set(ENCRYPT_IMAGE_CMD  --encrypt ${X25519_KEY_FILE})
message(STATUS "Using x25519 encrypted MCUBoot images.")
else()
message(STATUS "Using unencrypted MCUBoot images.")
# Make sure we clear if value is other than 1
set(ENCRYPT_IMAGES 0)
endif()

add_custom_command(
	TARGET ${EXECUTABLE} POST_BUILD
	COMMAND ${PYTHON} ${IMGTOOL} sign --header-size ${MCUBOOT_HEADER_SIZE} --align ${MCUBOOT_BOOT_MAX_ALIGN} --slot-size ${APP_SIZE} --version `cat ${CMAKE_CURRENT_BINARY_DIR}/version_string` --pad-header ${SIGN_IMAGE_CMD} ${ENCRYPT_IMAGE_CMD} ${EXECUTABLE}.bin ${EXECUTABLE}.dfu.bin
	BYPRODUCTS ${EXECUTABLE}.dfu.bin
	COMMENT "Creating MCUBoot image"
)

if(ENCRYPT_IMAGES STREQUAL 1)
add_custom_command(
	TARGET ${EXECUTABLE} POST_BUILD
	COMMAND ${PYTHON} ${IMGTOOL} sign --header-size ${MCUBOOT_HEADER_SIZE} --align ${MCUBOOT_BOOT_MAX_ALIGN} --slot-size ${APP_SIZE} --version `cat ${CMAKE_CURRENT_BINARY_DIR}/version_string` --pad-header ${SIGN_IMAGE_CMD} ${ENCRYPT_IMAGE_CMD} --clear ${EXECUTABLE}.bin ${EXECUTABLE}.clear.dfu.bin
	BYPRODUCTS ${EXECUTABLE}.clear.dfu.bin
	COMMENT "Creating clear MCUBoot image"
)
endif()

endif() # END USE_BOOTLOADER

# Use string len to check if a version is present or not
string(LENGTH "${VERSION}" VERSION_LEN)

if(VERSION_LEN GREATER 0)
set(CUSTOM_VERSION_STR "--version=${VERSION}")
else()
set(CUSTOM_VERSION_STR "")
endif()

# Only allow `make release` when -DRELEASE=1 is set for cmake
if(RELEASE STREQUAL "1")
add_custom_target(release
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/scripts/release/release.py ${EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR} ${CUSTOM_VERSION_STR} --syscfg ${APP_DIR}/app_config.yml
	DEPENDS ${EXECUTABLE}
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/release
)

# Don't allow `eng-release` when -DRELEASE=1
else()
add_custom_target(eng-release
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/scripts/release/release.py ${EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR} ${CUSTOM_VERSION_STR} --eng_build --syscfg ${APP_DIR}/app_config.yml
	DEPENDS ${EXECUTABLE}
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/release
)
endif()

add_custom_target(test
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/scripts/test/did_i_break_something.py ${CMAKE_SOURCE_DIR}/tools/scripts/test/configs
)

add_custom_target(cppcheck
	COMMAND cppcheck --inline-suppr ${SRC_DIR}/bsp ${SRC_DIR}/lib  ${APP_DIR}/
)

if (USE_BOOTLOADER STREQUAL 1)

# Flash MCUBoot image when using bootloader
if(ENCRYPT_IMAGES STREQUAL 1)
# If encryption is enabled and we're flashing to slot 0, we need to flash an unecrypted image
add_custom_target(flash
	COMMAND openocd -f ${DEBUG_SCRIPTS_PATH}/stlink-mod.cfg -f ${DEBUG_SCRIPTS_PATH}/st_nucleo_u5_hardreset.cfg -f ${DEBUG_SCRIPTS_PATH}/utils.cfg -c "program ${EXECUTABLE}.clear.dfu.bin ${APP_FLASH_ADDR} verify reset" -c "detach_swd" -c "exit"
	DEPENDS ${EXECUTABLE}.clear.dfu.bin
	COMMENT "Flashing clear image to target..."
)
else()
#If encryption is not enabled and we're flashing to slot 1, we flash the encrypted image
add_custom_target(flash
	COMMAND openocd -f ${DEBUG_SCRIPTS_PATH}/stlink-mod.cfg -f ${DEBUG_SCRIPTS_PATH}/st_nucleo_u5_hardreset.cfg -f ${DEBUG_SCRIPTS_PATH}/utils.cfg -c "program ${EXECUTABLE}.dfu.bin ${APP_FLASH_ADDR} verify reset" -c "detach_swd" -c "exit"
	DEPENDS ${EXECUTABLE}.dfu.bin
	COMMENT "Flashing image to target..."
)
endif()

# Flash MCUBoot image to secondary slot for testing
add_custom_target(flash_secondary
	COMMAND openocd -f ${DEBUG_SCRIPTS_PATH}/stlink-mod.cfg -f ${DEBUG_SCRIPTS_PATH}/st_nucleo_u5_hardreset.cfg -f ${DEBUG_SCRIPTS_PATH}/utils.cfg -c "program ${EXECUTABLE}.dfu.bin ${APP_IMG2_FLASH_ADDR} verify reset" -c "detach_swd" -c "exit"
	DEPENDS ${EXECUTABLE}.dfu.bin
	COMMENT "Flashing image to secondary slot"
)

# Create binary file with both the bootloader _and_ the main firmware
# This can be used to flash a brand new device with both at once

# TODO - figure out how to do unifed image with encrypted application. Do we put it in slot 1 and leave slot 0 blank?
if (DEFINED BOOTLOADER_PATH)
if(ENCRYPT_IMAGES STREQUAL 1)
add_custom_command(
	TARGET ${EXECUTABLE} POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} --gap-fill 0xFF --pad-to ${APP_FLASH_ADDR} -O binary ${BOOTLOADER_PATH} bootloader_padded.bin
	COMMAND cat bootloader_padded.bin ${EXECUTABLE}.clear.dfu.bin > ${EXECUTABLE}.unified.bin
	BYPRODUCTS bootloader_padded.bin
	BYPRODUCTS ${EXECUTABLE}.unified.bin
	COMMENT "Building unified image.(CLEAR)"
)
else()
add_custom_command(
	TARGET ${EXECUTABLE} POST_BUILD
	COMMAND ${CMAKE_OBJCOPY} --gap-fill 0xFF --pad-to ${APP_FLASH_ADDR} -O binary ${BOOTLOADER_PATH} bootloader_padded.bin
	COMMAND cat bootloader_padded.bin ${EXECUTABLE}.dfu.bin > ${EXECUTABLE}.unified.bin
	BYPRODUCTS bootloader_padded.bin
	BYPRODUCTS ${EXECUTABLE}.unified.bin
	COMMENT "Building unified image."
)
endif()

endif()

else()
# Flash non MCUBoot image
add_custom_target(flash
	COMMAND openocd -f ${DEBUG_SCRIPTS_PATH}/stlink-mod.cfg -f ${DEBUG_SCRIPTS_PATH}/st_nucleo_u5_hardreset.cfg -f ${DEBUG_SCRIPTS_PATH}/utils.cfg -c "program ${EXECUTABLE} verify reset" -c "detach_swd" -c "exit"
	DEPENDS ${EXECUTABLE}
	COMMENT "Flashing image to target..."
)
endif()

add_custom_target(debug
	COMMAND ${PYTHON} ${DEBUG_SCRIPTS_PATH}/debug.py ${EXECUTABLE} ${SRC_DIR} ${CMAKE_BINARY_DIR} "\"$ENV{CONDA_PREFIX}\""
	COMMENT "Debugging target"
)

add_custom_target(debug_rtos
	COMMAND ${PYTHON} ${DEBUG_SCRIPTS_PATH}/debug.py --rtos ${EXECUTABLE} ${SRC_DIR} ${CMAKE_BINARY_DIR} $ENV{CONDA_PREFIX}
	COMMENT "Debugging target with rtos support"
)

add_custom_target(debug_tui
	COMMAND ${PYTHON} ${DEBUG_SCRIPTS_PATH}/debug.py --tui ${EXECUTABLE} ${SRC_DIR} ${CMAKE_BINARY_DIR} $ENV{CONDA_PREFIX}
)

if (APP STREQUAL "bootloader")
# Added || exit 0 since dfu-flash reports an error even when it succeeds
# It says dfu-util: "Error during download get_status" after flashing and resetting
# This way make won't freak out
add_custom_target(dfu_flash
	COMMAND dfu-util -d 0483:df11 -c 1 -i 0 -a 0 -s ${FLASH_START}:leave -D ${EXECUTABLE}.bin || exit 0
	DEPENDS ${EXECUTABLE}
)
elseif(USE_BOOTLOADER STREQUAL 1)
add_custom_target(dfu_flash
	COMMAND dfu-util -d 0483:df11 -c 1 -i 0 -a 0 -s ${APP_FLASH_ADDR}:leave -D ${EXECUTABLE}.dfu.bin || exit 0
	DEPENDS ${EXECUTABLE}
)
else()
add_custom_target(dfu_flash
	COMMAND dfu-util -d 0483:df11 -c 1 -i 0 -a 0 -s ${APP_FLASH_ADDR}:leave -D ${EXECUTABLE}.bin || exit 0
	DEPENDS ${EXECUTABLE}
)
endif()

add_custom_target(memfault_upload
	COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/scripts/memfault/memfault_elf_uploader.py --software_type ${APP_NAME} ${EXECUTABLE}
	DEPENDS ${EXECUTABLE}
)

# Build app_config.cpp/h whenever app_config.yml changes
if(EXISTS ${APP_DIR}/app_config.yml)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(RELEASE_STR --release)
endif()

add_custom_command(
    OUTPUT ${APP_DIR}/app_config.cpp
           ${APP_DIR}/app_config.h
    DEPENDS ${APP_DIR}/app_config.yml
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/scripts/config/syscfg_gen.py
    COMMENT "Generating configuration files from app_config.yml"
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/scripts/config/syscfg_gen.py ${APP_DIR}/app_config.yml --out_dir ${APP_DIR} ${RELEASE_STR})
endif()

# Auto-generate app_error.yml if needed
# Get all files in the error_states subdirectory to add as dependencies
# Add ${APP_DIR}/app_error.yml in app CMakeLists.txt to include
# Also define ${APP_ERROR_STRUCT} in app CMakeLists.txt
file(GLOB ERROR_STATE_H "${SRC_DIR}/lib/errors/error_states/*.h")
add_custom_command(
    OUTPUT ${APP_DIR}/app_error.yml
    DEPENDS ${ERROR_STATE_H}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/scripts/error/error_parser.py
    DEPENDS ${APP_ERROR_STRUCT}
    COMMENT "Generating error configuration"
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/scripts/error/error_parser.py --ignore-dirty --struct-path ${APP_ERROR_STRUCT}  --yaml ${APP_DIR})

message(STATUS "Saving to ${CMAKE_CURRENT_BINARY_DIR}/version.c")


# Put together arguments for git_version.cmake command
set(GIT_VERSION_ARGS "-DAPP=${APP}")
list(APPEND GIT_VERSION_ARGS "-DSIGN_IMAGES=${SIGN_IMAGES}")
list(APPEND GIT_VERSION_ARGS "-DENCRYPT_IMAGES=${ENCRYPT_IMAGES}")
list(APPEND GIT_VERSION_ARGS "-DHW_VERSION=${HW_VERSION}")

if(VERSION_LEN GREATER 0)
message(STATUS "Using custom version ${VERSION}")
list(APPEND GIT_VERSION_ARGS "-DVERSION_STR=${VERSION}")
endif()

if(RELEASE STREQUAL "1")
list(APPEND GIT_VERSION_ARGS "-DRELEASE=1")
message(STATUS "RELEASE")
else()
list(APPEND GIT_VERSION_ARGS "-DRELEASE=0")
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
           ${CMAKE_CURRENT_BINARY_DIR}/_version.c
           ${CMAKE_CURRENT_BINARY_DIR}/version_string
    COMMAND ${CMAKE_COMMAND} ${GIT_VERSION_ARGS} -P ${CMAKE_SOURCE_DIR}/cmake/git_version.cmake)

# Always run this command on build to make sure conda env is active
# See link for more info: https://stackoverflow.com/a/32062884
add_custom_target(conda_check ALL COMMAND bash ${SCRIPTS_PATH}/util/conda_check.sh)
