# Getting Started

## Environment Setup

To get started developing custom applications for the Bristlemouth Development Kit, first you need to setup your developer environment.
- First, complete the [Initial Setup](../../../ENV_SETUP.md#initial-setup) section in the ENV_SETUP.md doc, then come back here. Stop when you get to the section titled "Building/Flashing - Command-line."
- Create a build directory, and build the Hello World App:
  ```bash
    bristlemouth $ conda activate bristlemouth
    bristlemouth $ mkdir cmake-build
    bristlemouth $ cd cmake-build
    cmake-build $ mkdir hello_world
    hello_world $ cd hello_world
    hello_world $ cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi-gcc.cmake -DBSP=bm_mote_v1.0 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_APP_TYPE=BMDK -DAPP=hello_world

    ->
    -- The C compiler identification is GNU 10.3.1
    ...
    -- Build files have been written to: /Users/evan/workspace/bristlemouth/cmake-build/hello_world

    hello_world $ make -j

    ->
    [  0%] Generating bs_stm32u575.lds
    [  0%] Built target conda_check
    [  0%] Building C object src/apps/bm_devkit/hello_world/CMakeFiles/lwipcore.dir/__/__/__/third_party/lwip/src/core/def.c.obj
    ...
    [100%] Building C object src/CMakeFiles/bm_mote_v1.0-hello_world-dbg.elf.dir/version.c.obj
    [100%] Linking CXX executable bm_mote_v1.0-hello_world-dbg.elf
    text	   data	    bss	    dec	    hex	filename
    209156	   1824	 451566	 662546	  a1c12	bm_mote_v1.0-hello_world-dbg.elf
    Creating MCUBoot image
    [100%] Built target bm_mote_v1.0-hello_world-dbg.elf

    ### SUCCESS!
    ```
- Once completed you can successfully start building applications. You're ready to develop and flash custom firmware to your Dev Kit's Mote.
  There are a variety of ways to load firmware onto a Mote, a detailed overview can be found in the
  [Updating Firmware of A Development Kit](https://sofarocean.notion.site/Mote-Firmware-Updates-ef18d826d8834a88b88b98163bba884e?pvs=4)
  section of the online docs.

## Applications Overview

Developing in FreeRTOS for the powerful STM32U5 platform can be daunting for those used to lightweight coding in
simpler frameworks like Arduino or Python.
The example applications for the Bristlemouth Development Kit are intended to:
- Demonstrate core features of Bristlemouth and the Dev Kit.
- Provide simple reference examples for the types of applications users will want to develop on top of the Dev Kit.
- Create Arduino-style sandboxed starting points that make it easy for people with a wide range of development experience to dig in and start experimenting.

We will continuously iterate and improve on these examples adding demonstrations of more Bristlemouth features,
Micro Python support, useful tools and OSS contributions, examples using external Application Processors like Raspberry Pi, etc.
If you have ideas of examples you'd like to see (or better yet - help develop!) please chime in on the
[Bristlemouth community forums](https://bristlemouth.discourse.group/).

### Hello World
The Hello World App demonstrates basic Bristlemouth and Dev Kit functionality:
- Run the Bristlemouth and Dev Kit USB CLIs.
- Run the on-board power monitors, humidity, temperature and pressure sensors:
    - Send this data to Spotter using Bristlemouth.
    - _see src/apps/bm_devkit/bmdk_common/sensors/sensors.cpp for an entry point to dig into how this works._
    - _doesn't run the on-board IMU yet._
- Subscribe to UTC time data from Spotter over Bristlemouth, and set the Real Time Clock from this data.
    - _Spotter only publishes this data when it has a GPS signal fix._
- Demonstrate some simple Blink code in the user code loop.

### Serial Payload Example
The Serial Payload Example App builds on top of the Hello World App by adding functionality to interact with a sensor
on the Low Power Payload UART Interface. This interface drives the RS232, RS485, SDI-12, and UART interfaces of the Dev Kit.
See [Bristlemouth Dev Kit Guide 3 Integrating a Serial Sensor](https://bristlemouth.notion.site/Bristlemouth-Dev-Kit-Guide-3-Exploring-Bristlemouth-Features-f08a56dc01ac47d889a4eacb9f4904f8?pvs=4)
in the online docs for an example walk-through.
- Initialize and read data from the Payload UART.
  - Use an ASCII serial line buffer.
- Vbus & Vout power supply control.

### RBR Coda3 Integration
The RBR Coda3 Integration App builds on top of the Serial Payload Example App by implementing a basic parsing and analysis
for reading data from a popular marine depth sensor - the RBR Coda3.D. Follow along the process of building the app in
the [Bristlemouth Dev Kit Guide 3 Integrating a Serial Sensor](https://bristlemouth.notion.site/Bristlemouth-Dev-Kit-Guide-3-Exploring-Bristlemouth-Features-f08a56dc01ac47d889a4eacb9f4904f8?pvs=4).
- Parsing text into numeric values.
- Aggregating basic statistics.
- Using Bristlemouth from user_code to print to Spotter's USB console, SD card, and transmit data over cellular or satellite.
- An example of thread safety using a FreeRTOS Mutex.

### I2C Sensor Integration - _Coming Soon!_

### Custom Bristlemouth Configurations - _Coming Soon!_

### High Data Rate Cellular Telemetry - _Coming Soon!_

### More Sensor Integration Examples and Tools - _Coming Soon!_
- Multiple LineParser schemas for a single sensor interface.
- Implementing command interfaces to serial sensors.
- Kahan sums, Welford variance, online filters, and other numerical & DSP techniques.
- Examples with other interfaces - RS485, SDI-12, SPI, UART.

## Developing New Applications
In each Dev Kit example application, there is an **app_main.cpp** file that has common boilerplate.
This is initializing Bristlemouth, parts of the Mote processor, CLIs, and hardware peripherals like memory and I/O expanders.
This also sets up all the FreeRTOS _tasks (like threads on a computer)._ Unless you're an experienced emedded C/C++ developer,
you probably shouldn't mess with app_main to start with.

There is a **user_code.cpp** file that has Arduino-style `setup()` (called once after everything else initializes),
and `loop()` (called repeatedly forever) functions. These functions run in a lower priority user task so you don't have
to worry about blocking delays in these functions interfering with other parts of the system. You _do_ have to worry about
thread safety some times, as some of the code in example app user_code.cpp files are actually called from other tasks.
These cases will be called out in code comments.

### Building Example Applications
- `cd` into `cmake-build/exact_name_of_app_directory`
  - _Create directories with `mkdir` or `mkdir -p` as needed along the way_.
- Run the following cmake command, modifying it by changing the last argument to the exact name of the app directory you're building:
  - `cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi-gcc.cmake -DBSP=bm_mote_v1.0 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_APP_TYPE=BMDK -DAPP=exact_name_of_app_directory`
- Then `make`, `make flash`, or `make dfu_flash` per however you're deploying firmware to your Motes. _See [Updating Firmware of A Development Kit](https://sofarocean.notion.site/Mote-Firmware-Updates-ef18d826d8834a88b88b98163bba884e?pvs=4) for more details._

### Creating Your Own Applications
When you're experimenting and testing, it's easiest to just modify the example apps for quick prototyping.
However, if and when you get to the point of wanting to develop custom C/C++ applications on top of this application framework,
we recommend:
- Copy the entire app directory of the example app you're basing your app on.
- Paste this entire directory into the `bm_devkit` folder, giving it a unique name (_no spaces or special characters please_).
- Then create a build directory for your app per the process above.
This makes it simple to keep pulling in the latest versions of Bristlemouth and the example applications from GitHub
without having to deal with merge conflicts.
