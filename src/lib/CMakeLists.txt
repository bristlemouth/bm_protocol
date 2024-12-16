set(LIB_DIR ${CMAKE_CURRENT_LIST_DIR})

# Include TinyUSB
add_subdirectory(${LIB_DIR}/usb)

# Include BCMP
add_subdirectory(${LIB_DIR}/bm_integration)

# Include BM_NCP files
add_subdirectory(${LIB_DIR}/bm_ncp)

# Set bm_serial specific defines and add to project
set(BM_SERIAL_COMPILER_FLAGS ${COMPILE_FLAGS})
include(${LIB_DIR}/bm_serial/CMakeLists.txt)
target_link_libraries(bm_serial PUBLIC bmcore)

set(LIB_FILES
    ${LIB_DIR}/cli/cli.c
    ${LIB_DIR}/common/avgSampler.cpp
    ${LIB_DIR}/common/bootloader_helper.c
    ${LIB_DIR}/common/device_info.c
    ${LIB_DIR}/common/differenceSignal.cpp
    ${LIB_DIR}/common/enumToStr.c
    ${LIB_DIR}/common/Exo3LineParser.cpp
    ${LIB_DIR}/common/external_flash_partitions.c
    ${LIB_DIR}/common/freertos_cpp_overrides.cpp
    ${LIB_DIR}/common/freertos_support.c
    ${LIB_DIR}/common/gpioISR.c
    ${LIB_DIR}/common/libc_overrides.c
    ${LIB_DIR}/common/LineParser.cpp
    ${LIB_DIR}/common/OrderedKVPLineParser.cpp
    ${LIB_DIR}/common/OrderedSeparatorLineParser.cpp
    ${LIB_DIR}/common/lpm_u5.c
    ${LIB_DIR}/common/lptimTick_u5.c
    ${LIB_DIR}/common/nvmPartition.cpp
    ${LIB_DIR}/common/pcap.c
    ${LIB_DIR}/common/trace.c
    ${LIB_DIR}/common/reset_reason.c
    ${LIB_DIR}/common/serial.c
    ${LIB_DIR}/common/sensorWatchdog.cpp
    ${LIB_DIR}/common/serial_console_u5.cpp
    ${LIB_DIR}/common/stress.c
    ${LIB_DIR}/common/tokenize.c
    ${LIB_DIR}/common/uptime.c
    ${LIB_DIR}/common/util.c
    ${LIB_DIR}/common/watchdog.c
    ${LIB_DIR}/sys/ram_partitions.c
    ${LIB_DIR}/sys/bm_config_wrapper.cpp
    ${LIB_DIR}/debug/debug.c
    ${LIB_DIR}/debug/debug_spotter.cpp
    ${LIB_DIR}/debug/debug_gpio.c
    ${LIB_DIR}/debug/debug_htu.cpp
    ${LIB_DIR}/debug/debug_i2c.c
    ${LIB_DIR}/debug/debug_spi.c
    ${LIB_DIR}/debug/debug_pluart_cli.cpp
    ${LIB_DIR}/debug/debug_pressure_sensor.cpp
    ${LIB_DIR}/debug/debug_memfault.c
    ${LIB_DIR}/debug/debug_rtc.c
    ${LIB_DIR}/debug/debug_ina232.cpp
    ${LIB_DIR}/debug/debug_sys.cpp
    ${LIB_DIR}/debug/debug_w25.cpp
    ${LIB_DIR}/debug/debug_nvm_cli.cpp
    ${LIB_DIR}/debug/debug_dfu.cpp
    ${LIB_DIR}/debug/debug_bm_service.cpp
    ${LIB_DIR}/debug/debug_configuration.cpp
    ${LIB_DIR}/debug/debug_tca9546a.cpp
    ${LIB_DIR}/drivers/abstract/abstract_i2c.cpp
    ${LIB_DIR}/drivers/abstract/abstract_spi.cpp
    ${LIB_DIR}/drivers/bm_dfu_wrapper.cpp
    ${LIB_DIR}/drivers/bm_rtc_wrapper.c
    ${LIB_DIR}/drivers/adin2111/src/adi_hal.c
    ${LIB_DIR}/drivers/bme280driver.cpp
    ${LIB_DIR}/drivers/htu21d.cpp
    ${LIB_DIR}/drivers/ina232.cpp
    ${LIB_DIR}/drivers/nau7802.cpp
    ${LIB_DIR}/drivers/ms5803.cpp
    ${LIB_DIR}/drivers/pca9535.c
    ${LIB_DIR}/drivers/protected/protected_i2c.c
    ${LIB_DIR}/drivers/protected/protected_spi.c
    ${LIB_DIR}/drivers/stm32_adc.c
    ${LIB_DIR}/drivers/stm32_io.c
    ${LIB_DIR}/drivers/stm32_rtc.c
    ${LIB_DIR}/drivers/TSYS01.cpp
    ${LIB_DIR}/drivers/w25.cpp
    ${LIB_DIR}/drivers/tca9546a.cpp
    ${LIB_DIR}/lwip/lwip_support.c
    ${LIB_DIR}/mcuboot/mcuboot_cli.c
    ${LIB_DIR}/mcuboot/port_flash.c
    ${LIB_DIR}/mcuboot/port_misc.c
    ${LIB_DIR}/memfault/memfault_platform_core_u5.c
    ${LIB_DIR}/sensor_sampler/sensorSampler.cpp
    ${LIB_DIR}/sensor_sampler/powerSampler.cpp
    ${LIB_DIR}/sensor_sampler/htuSampler.cpp
    ${LIB_DIR}/sensor_sampler/loadCellSampler.cpp
    ${LIB_DIR}/sensor_sampler/pressureSampler.cpp
    ${TINYUSB_FILES}
    ${BM_SERIAL_FILES}
    ${BM_INTEGRATION_FILES}
)

if(APP_NAME MATCHES "bridge" AND
   NOT APP_NAME MATCHES "bringup")
    list(APPEND LIB_FILES
        ${LIB_DIR}/debug/debug_bridge_power_controller.cpp
        ${BM_NCP_FILES}
    )
endif()

if(APP_NAME MATCHES "bringup")
    list(APPEND LIB_FILES
        ${LIB_DIR}/debug/debug_uart.c
        ${LIB_DIR}/debug/debug_adin_raw.c
    )
endif()

if(CMAKE_APP_TYPE STREQUAL "BMDK" OR
   CMAKE_APP_TYPE STREQUAL "bristleback" OR
   CMAKE_APP_TYPE STREQUAL "rs232_expander" OR
   APP_NAME STREQUAL "bringup_mote" OR
   APP_NAME STREQUAL "mote_bristlefin" OR
   APP_NAME STREQUAL "bm_soft_module")
    if(NOT CMAKE_APP_TYPE STREQUAL "bristleback" AND
       NOT APP_NAME STREQUAL "bm_soft_module" AND
       NOT CMAKE_APP_TYPE STREQUAL "rs232_expander")
        list(APPEND LIB_FILES
            ${LIB_DIR}/bristlefin/bristlefin.cpp
        )
    endif()
    if(NOT APP_NAME STREQUAL "bringup_mote")
        list(APPEND LIB_FILES
            ${LIB_DIR}/common/payload_uart.cpp
        )
    endif()
endif()

set(LIB_INCLUDES
    ${LIB_DIR}/bristlefin
    ${LIB_DIR}/bm_integration
    ${LIB_DIR}/bridge
    ${LIB_DIR}/cli
    ${LIB_DIR}/common
    ${LIB_DIR}/debug
    ${LIB_DIR}/drivers
    ${LIB_DIR}/drivers/abstract
    ${LIB_DIR}/drivers/adin2111/include
    ${LIB_DIR}/drivers/include
    ${LIB_DIR}/drivers/protected
    ${LIB_DIR}/lwip
    ${LIB_DIR}/mcuboot
    ${LIB_DIR}/memfault
    ${LIB_DIR}/middleware
    ${LIB_DIR}/sensor_sampler
    ${LIB_DIR}/sys/
    ${SRC_DIR}/third_party/
    ${SRC_DIR}/third_party/aligned_malloc/
    ${SRC_DIR}/third_party/cobs-c
    ${SRC_DIR}/third_party/crc
    ${SRC_DIR}/third_party/fnv/
    ${SRC_DIR}/third_party/FreeRTOS-Plus-CLI/
    ${SRC_DIR}/third_party/printf/
    ${SRC_DIR}/third_party/tinycbor/src
    ${BM_INTEGRATION_INCLUDES}
    ${TINYUSB_INCLUDES}
    ${BM_SERIAL_INCLUDES}
)

if(APP_NAME MATCHES "bridge" AND
   NOT APP_NAME MATCHES "bringup")
    list(APPEND LIB_INCLUDES
        ${BM_NCP_INCLUDES}
    )
endif()

set(LIB_DEFINES
    CFG_TUSB_MCU=OPT_MCU_STM32U5
)

set(LIB_LIBS "")

set(LIB_DIR "${LIB_DIR}" PARENT_SCOPE)
set(LIB_DEFINES "${LIB_DEFINES}" PARENT_SCOPE)
set(LIB_LIBS "${LIB_LIBS}" PARENT_SCOPE)
set(LIB_FILES "${LIB_FILES}" PARENT_SCOPE)
set(LIB_INCLUDES "${LIB_INCLUDES}" PARENT_SCOPE)
