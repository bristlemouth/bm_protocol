# Bristlemouth Developer Guide

This document (hopefully) details the where/why/how of bristlemouth firmware. For instructions of development environment setup as well as flashing/debugging, see the main [README.md](README.md) file.

If you're developing on the Bristlemouth Development Kit, see the [BMDK_README.md](./src/apps/bm_devkit/BMDK_README.md).

# Directory Structure
Where is everything?!

## [src/](src/)
The src directory contains all of the firmware source files for the project.

### [src/apps/])(src/apps/)
This directory contains all of the separate applications in their own directories.

### [src/bsp/])(src/bsp/)
This directory contains all of the different Board Support Packages(BSP's) for all of the different hardware revisions/configurations. Whenever you want to change peripheral mapping, IO pins, etc, you'll have to look in here.

We're currently using [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) to do the initial pin/peripheral configuration and code generation. We haven't yet documented that process, so if you need help, reach out to the firmware team.

### [src/lib/])(src/lib/)
The lib directory has most of the source code in the project. This is where you'll find most drivers/libraries.

### [src/third_party/])(src/third_party/)
This directory contains all third party libraries (usually included as git submodules) used by the project.

## [test/](test/)
The test directory contains all of the unit tests for the project.

## [tools/](tools/)
The tools directory is primarily comprised of scripts and various tools to help with development.

# Quickstart Guide

## Adding new files to an application
### New files inside the app directory
Each app directory has a CMakeLists.txt file where most of the action happens.
To add files located in the **src/apps/appname/** directory, look for the APP_FILES CMake variable in the project. For example:
```
set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external_flash_partitions.c
#
#   Insert new file here! (For example)
#
    ${MCUBOOT_FILES}
    ${FREERTOS_FILES}
    ${LIB_FILES}
    ${BRISTLEMOUTH_FILES}
    ${LWIP_DIR}/contrib/ports/freertos/sys_arch.c
    ${MEMFAULT_SOURCES}
    ${TINYUSB_FILES}
    PARENT_SCOPE)
```

### New library files
Library files should be added under the LIB_FILES CMake variable. For example:
```
set(LIB_FILES
    ${SRC_DIR}/lib/cli/cli.c
    ${SRC_DIR}/lib/common/bootloader_helper.c
    ${SRC_DIR}/lib/common/device_info.c
#
#   Insert new file here! (For example)
#
    ${SRC_DIR}/lib/common/enumToStr.c
```

Don't forget to also add the include directory to LIB_INCLUDES, otherwise you'll have to use the full path for including files (which is ok too, for example `#include "lib/bm_serial/crc.h"`).

## defaultTask
Since bristlemouth uses FreeRTOS, most of the work happens inside tasks. The defaultTask is the first task that gets created. All other tasks are created from it (or from one of the newly created ones.)

The defaultTask lives inside app_main.cpp inside each app directory.

### Task Priorities
All tasks have their own priority level. These should all be defined in the `task_priorities.h` file in each app directory.

### ISR's
Any FreeRTOS call that happens inside an interrupt service routine MUST be of the type `<functionName>FromISR()`, otherwise, you're going to have a bad time.


## IO
Toggling GPIOs is done via the io.h library. The functions IOWrite and IORead will work on both STM32 pins as well as IO expander pins. The pins are defined in `bsp.h` and `bsp_pins.c` in the hardware specific BSP directory.


## Adding CLI commands
We're currently using the FreeRTOS_CLI system for handling CLI commands. In general, each command, or set of commands, lives in a separate file and uses the `FreeRTOS_CLIRegisterCommand` function to register it with the system.

The best way to get started is to copy a file from the [src/lib/debug/](src/lib/debug/) directory and make a new command. Don't forget to call the init function (which calls `FreeRTOS_CLIRegisterCommand`) somewhere in your app_main's defaultTask.

###
