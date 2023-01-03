cmake_minimum_required(VERSION 3.18)

# Add sdk files as suggest here:
# https://github.com/memfault/memfault-firmware-sdk#add-sources-to-build-system
set(MEMFAULT_DIR ${CMAKE_SOURCE_DIR}/src)
set(MEMFAULT_SDK_ROOT ${CMAKE_SOURCE_DIR}/src/third_party/memfault-firmware-sdk)
list(APPEND MEMFAULT_COMPONENTS "core" "util" "panics" "metrics" "http")
include(${MEMFAULT_SDK_ROOT}/cmake/Memfault.cmake)
memfault_library(${MEMFAULT_SDK_ROOT} MEMFAULT_COMPONENTS
 MEMFAULT_COMPONENTS_SRCS MEMFAULT_COMPONENTS_INC_FOLDERS)

set(MEMFAULT_PORT_ROOT ${MEMFAULT_SDK_ROOT}/ports)
set(MEMFAULT_FREERTOS_PORT_ROOT ${MEMFAULT_PORT_ROOT}/freertos)
# set(MEMFAULT_STM32_PORT_ROOT ${MEMFAULT_PORT_ROOT}/stm32cube/l4)
set(MEMFAULT_STM32_PORT_ROOT ${MEMFAULT_PORT_ROOT}/stm32cube/u5)

set(MEMFAULT_SOURCES
    ${MEMFAULT_COMPONENTS_SRCS}

    # FreeRTOS Port
    ${MEMFAULT_FREERTOS_PORT_ROOT}/src/memfault_core_freertos.c
    ${MEMFAULT_FREERTOS_PORT_ROOT}/src/memfault_freertos_ram_regions.c
    ${MEMFAULT_FREERTOS_PORT_ROOT}/src/memfault_metrics_freertos.c
    ${MEMFAULT_FREERTOS_PORT_ROOT}/src/memfault_panics_freertos.c

    # STM32L4 Cube Ports
    ${MEMFAULT_STM32_PORT_ROOT}/flash_coredump_storage.c
    ${MEMFAULT_STM32_PORT_ROOT}/rcc_reboot_tracking.c

    # Custom platform port
    # ${MEMFAULT_DIR}/lib/memfault/memfault_platform_coreorm_core.c
    )

set(MEMFAULT_INCLUDES
    ${MEMFAULT_COMPONENTS_INC_FOLDERS}
    ${MEMFAULT_DIR}/
    ${MEMFAULT_FREERTOS_PORT_ROOT}/include
    ${MEMFAULT_PORT_ROOT}/include
    )

# Deal with unused-parameter warnings for memfault sdk
set_source_files_properties(
    ${MEMFAULT_SDK_ROOT}/components/panics/src/memfault_fault_handling_arm.c
    ${MEMFAULT_FREERTOS_PORT_ROOT}/src/memfault_metrics_freertos.c
    ${MEMFAULT_FREERTOS_PORT_ROOT}/src/memfault_panics_freertos.c
    ${MEMFAULT_STM32_PORT_ROOT}/flash_coredump_storage.c
    DIRECTORY ${SRC_DIR}
    PROPERTIES
    COMPILE_FLAGS -Wno-unused-parameter)