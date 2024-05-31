set(BSP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(COMMON_BSP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
set(BSP_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bsp_pins.c
        # ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/adc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/gpio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/i2c.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/icache.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/iwdg.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/rtc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/spi.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32u5xx_hal_msp.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32u5xx_hal_timebase_tim.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32u5xx_it.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32u5xx.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/usart.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/usb_otg.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/gpdma.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c
        # ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_adc.c
        # ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_adc_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_cortex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_exti.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gpio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gtzc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_icache.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pcd.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pcd_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim_ex.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_exti.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_lpgpio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_lpuart.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_gpio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_rcc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_rtc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_usart.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_usb.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_utils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_lptim.c
        ${COMMON_BSP_DIR}/stm32/startup_stm32u575zitxq.c
        PARENT_SCOPE)
set(BSP_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32U5xx/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc
        ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy
        PARENT_SCOPE)

set(BSP_DEFINES
        USE_FULL_LL_DRIVER
        USE_HAL_DRIVER
        STM32U575xx
        MEMFAULT_COREDUMP_STORAGE_WRITE_SIZE=512
        BSP_MOTE_BRISTLEBACK_V1_0
        MEMFAULT_SOFWARE_WATCHDOG_SOURCE=LPTIM2_BASE
        MEMFAULT_EXC_HANDLER_WATCHDOG=LPTIM2_IRQHandler
        PARENT_SCOPE)

set(BSP_CPU_FLAGS
    -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
    PARENT_SCOPE)

# Deal with library file warnings
set_source_files_properties(

            ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr.c
            ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c

            DIRECTORY ${SRC_DIR}

            PROPERTIES
            COMPILE_FLAGS -Wno-unused-parameter)
