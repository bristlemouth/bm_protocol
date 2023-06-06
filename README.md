# Bristlemouth Firmware

## Developer Guide (in progress)
If you already have your development environment set up, check out the [DEVELOPER.md](DEVELOPER.md) file for some quick start guides, etc...

## Initial setup
### Install conda

[Conda](https://docs.conda.io/en/latest/) is used to manage dependencies (compiler, openocd, cmake, python) so that everyone has the same version of all the tools (without affecting the rest of your system).

If you don't have conda installed, follow these steps:

```
$ cd /tmp
# Download for Mac:
$ wget https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-x86_64.sh
# Download for Linux:
$ wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh
# install it
$ bash Miniconda3-latest-*-x86_64.sh
```

(If you're running on a raspberry pi with ubuntu 64-bit, you'll need to use [miniforge](https://github.com/conda-forge/miniforge) instad of miniconda.)

Tell the installer that, yes, you do want to initialize miniconda

**NOTE:** you'll have to relaunch your terminal for conda to start working.

You'll notice that the new terminal will be inside the conda (base) environment, if you don't want this to happen automatically, just run `conda config --set auto_activation_base false` once and relaunch.

### Extra Linux Setup

For now, we have to use gcc that comes with Linux to build the tests

`sudo apt install gcc g++`

If you plan on flashing/debugging with linux, you'll also need the following
`sudo apt install libncurses5 libncurses5-dev python2.7 python2.7-dev`

Don't forget to copy the [udev rules](tools/udev) so the STLink will work!

### Set up environment

This will create a conda environment and download/install all the dependencies.
You will have to activate this specific environment whenever you're working inside this repo.

```
$ conda create -n bristlemouth
$ conda activate bristlemouth
$ conda env update -f environment.yml
```

Once you're done, you can always deactivate the environment with:
```
$ conda deactivate
```

Whenever you pull new project changes (specifically those that update environment.yml), you should run `conda env update -f environmnent.yml` again.

### Set up Pre-commit

This will install helpful Pre-commit checks to make your code safer and more "beautiful". Make sure to have your conda env updated and active before running this command.

```
pre-commit install
```
You can also run the checks whenever you want by running:
```
pre-commit run
```

Note that if pre-commit modifies your code, you will need to `git add` the files before you commit them.
If you'd like to force a commit through, use the `git commit --no-verify` flag.~

### Set up environment variables
The environment variables needed for the project are listed in `.env.example`. You'll need to gather this information and put them in a `.env` file.

### Pull submodules
Bristlemouth uses several submodules to include third party libraries, etc. These files might not have been downloaded when cloning/pulling. If that's the case, run `git submodule update --init` to download them.

A couple things to note about submodules
- The submodule folders do not update SHAs automatically. If we need updated scripts, run `git submodule update --remote path/to/submodule`


## Building/Flashing - Command-line

### Configure CMake

This follows a pretty standard cmake process. First you create a directory for this particular build/configuration. You can either create a top level directory for each build (/cmake-build-debug, /cmake-build-mote_bristlefin) or one top level directory with individual build subdirectories (/cmake-build/bristlemouth, /cmake-build/mote_bristlefin). The `cmake-build` prefix is in the .gitignore, so however you structure the different folders they should still begin with that.

```
# Sub-directory example
$ mkdir -p cmake-build/mote_bristlefin
$ cd cmake-build/mote_bristlefin
```

Then you run cmake and tell it where the toolchain file is located, which BSP(board support package) to use, as well as which application. Pay attention to the `../` relative paths in the args - depending on how your cmake build directories are set up, you might have to replace them with `../..`.


```
# If you have individual top-level build directores for each app
$  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-gcc.cmake -DBSP=bm_mote_v1.0 -DAPP=mote_bristlefin -DCMAKE_BUILD_TYPE=Debug

# If you have one top-level build dir with different sub-dirs for each app
$  cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi-gcc.cmake -DBSP=bm_mote_v1.0 -DAPP=mote_bristlefin -DCMAKE_BUILD_TYPE=Debug
```

You can optionally set the build type to release (it defaults to Debug) by appending `-DCMAKE_BUILD_TYPE=Release` to the command.

### Building the Project

Once inside the build directory you can just run:

```
make
```

or

```
$ cmake --build .
```

If you want to enable parallel builds, add `-j` to either of those commands.

### Bootloader
Before flashing your application, you **MUST** first build/flash the bootloader. This only needs to happen once per device.

To configure:

```
$ mkdir -p cmake-build/bootloader
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-gcc.cmake -DBSP=bootloader -DAPP=bootloader -DCMAKE_BUILD_TYPE=Release
```

To build and flash (`dfu_flash` can also be used here):

```
make -j flash
```


### Flashing/Debugging
To flash using openOCD/STLink (more debugger support to be added later), you can just run:

**NOTE: You will need to flash the bootloader before you can use any application. This only has to be done once**

```
$ make flash
```

To run the GDB debugger, you can use:

```
$ make debug
```

After flashing, you should see the red and blue lights blinking on the STM32U575 Nucleo development board. You can also open up a usb/serial terminal to see the printf output. For example on MacOS: `pyserial-miniterm /dev/cu.usbmodem0000083039671`

#### USB dfu-util

You can also take advantage of the built-in usb bootloader in the STM32U575 to flash the device. You'll need to have [dfu-util](http://dfu-util.sourceforge.net/) installed. On MacOS just `brew install dfu-util`, on Ubuntu just `apt-get install dfu-util` (I do hope to add it to the conda environment, but it's not a high priority right now.)

Once the device is in DFU mode, you can run `dfu-util -l` to see if the device is detected. To flash it with the latest firmware, just run `make dfu_flash`!

`make dfu_flash` can be used with both the bootloader and main applications.


### Uploading .elf to Memfault

After building the project, run `make memfault_upload`. Make sure your .env file is set up ahead of time!

### Uploading memfault coredump over GDB
If you're in GDB and a crash occurs, this is how you capture a coredump to upload to memfault (make sure you already ran `make memfault_upload`!) :

In GDB:
`source ../../../src/third_party/memfault-firmware-sdk/scripts/memfault_gdb.py`

then

`memfault login alvaro.prieto@sofarocean.com <your memfault API key goes here> -o sofar-ocean -p bristlemouth`

finally

`memfault coredump`

## Building/Flashing - CLion

### Configuring CLion
Written using CLion 2020.3.1

Open the preferences window (**⌘ + ,**) and go to **Build, Execution, Deployment->Toolchains**. Add a new toolchain (**⌘ + N**) and select `System`. Name it **bristlemouth**.

For CMake, click the `...` beside the dropdown and navigate to `/your/miniconda3/envs/bristlemouth/bin` and select `cmake`. The resulting path that gets displayed in the dropdown might look something like `~/miniconda3./envs/bristlemouth/xpack-openocd/bin/cmake`. This is ok because the former `cmake` is an alias of the latter.

Do the same as above for Make and Debugger. The paths should look similar to below:
- Make: `~/miniconda3/envs/bristlemouth/bin/make`
- Debugger: `~/miniconda3/envs/bristlemouth/xpack-openocd/bin/arm-none-eabi-gdb`

Now go to the CMake preferences (**Build, Execution, Deployment->CMake**) and select the Debug profile.
- Update the `Toolchain` dropdown to `bristlemouth`
- Copy the below to CMake options (be sure to update the path to your miniconda3!):
  - `-DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake -DCOMPILER_PATH=/Users/genton/miniconda3/envs/bristlemouth/xpack-openocd/bin/ -DBSP=nucleo_u575 -DAPP=bringup`

Finally go to the Embedded Development preferences (**Build, Execution, Deployment->Embedded Development**). This time you need to set the **OpenOCD Location**. Unfortunately, this doesn't seem to like relative paths, so you'll have to browse manually to the openocd file in `~/miniconda3/envs/bristlemouth/bin/openocd`. If you click on **Test** next to the path, you should see a green box with something like `xPack OpenOCD, x86_64 Open On-Chip Debugger 0.10.0+dev (2020-10-13-20:30)`.

Click Apply and Ok. CMake should automatically reload now. If it fails, delete the **cmake-build-debug-xmoonjelly** folder and try reloading again.

To build/flash an app, select `flash` from the dropdown at the top and press (**Control ^ + R**). If a window pops up, click the **Executable** dropdown, select `bristlemouth-bringup.elf`, and then click Apply and Ok.

**NOTE:** Since we strip absolute paths from the build files, GDB needs to know where they are to show you source files while debugging. [CLion recommends](https://www.jetbrains.com/help/clion/configuring-debugger-options.html#gdbinit-lldbinit) using a .gdbinit file for this. The project .gdbinit is included in the git repo, but you must add this to your global `~/.gdbinit` file for it to work:
```
set auto-load local-gdbinit on
add-auto-load-safe-path /
```

To access the serial, we normally use `pyserial-miniterm` located in the conda env. However since we aren't loaded in the environment, we'll need to call the script directly. Here's a command that will add `pyserial-miniterm` as an alias `mini` to your bash profile so you can use it from anywhere (might be different if using zsh)
```
$ echo -e "\nalias mini='~/miniconda3/envs/bristlemouth/bin/pyserial-miniterm'" >> ~/.bash_profile
```
To run,
```
mini /dev/tty.usbmodem2058305D54521 115200
```

### Building/Flashing/Debugging

To build the project, you can just use the top menu **Build->Build**.

To flash/debug, make sure the **OpenOCD Flash/Debug** run/debug configuration is selected (Menu next to the play button and debugger button). You can now use the **Run->Run 'OpenOCD/Flash/Debug** and **Run->Debug 'OpenOCD/Flash/Debug** to flash and debug the project.

#### Flashing with a specific STLink device
If you have more than one STLink debugger connected to your computer, you must tell the flashing script which one to use. The way to do this is as follows:

```STLINK_SERIAL=004D00423438510B34313939 make flash```

To get the serial number on MacOS, you'll have to go to system information and look in the USB device tree for the STLink device. There should be a Serial Number field for each one.

In Linux, use `udevadm info /dev/ttyACM2 | grep ID_SERIAL_SHORT` or `lsusb -v -d 0483:374b | grep iSerial` (on the appropriate device)

### Uploading .elf to Memfault
#### Configure CLion to use the conda environment's python version
Open the preferences window (**⌘ + ,**) and go to **Build, Execution, Deployment->Python Interpreter**. Click the gear in the top right corner of the window and select **Add**. On the list on the left, select **Conda Environment->Existing environment**. For the interpreter, we'll want to point to the path of the python3 within the bristlemouth environment. If you installed conda at the default directory, that path should be `~/miniconda/envs/bristlemouth/bin/python3`. **Conda executable** should point to `~/miniconda/bin/conda`. Click OK and make sure the **Python Interpreter** is filled in and is pointing to the correct path.

#### Adding .elf uploads to builds
Click the dropdown at the top of the CLion window (next to the Build/Run buttons) and select **Edit Configurations**. In the top left corner, click the **+** and select **Python**.

Fill in these settings and then hit Apply (not OK!):
- Name: `memfault_elf_uploader`
- Script path: `/path/to/fleet-bristlemouth-fw/tools/scripts/memfault/memfault_elf_uploader.py`
- Python interpreter: Select **Use specified interpreter** and select the python interpreter we created in the previous step
- Working directory: `/path/to/fleet-bristlemouth-fw/tools/scripts/memfault/`

To add this script to a build, select a CMake Application from the dropdown list on the left side. Under the **Before launch** section, click **+** and select **Run Another Configuration->memfault_elf_uploader** and then click Apply and OK.

If you try building and you get an error on a line with an environment variable, you might have to fully exit CLion and then reopen it so that it can recognize newly created environment variables.

## Unit Tests

We're using [GoogleTest](https://github.com/google/googletest) to run the unit tests. At the moment, the tests are compiled and run natively on the host machine. The goal, eventually, is to compile them with the ARM compiler and run them on an emulator such as [Renode](https://renode.io/).

**Note:** Linux and MacOS aren't using the exact same compiler to run the tests. They're both clang, but MacOS uses the builtin one and Linux the one from the Conda envrionment. It's not a huge issue so I'm not going to spend the time to debug that right now.

### Configure CMake

This follows a pretty standard cmake process. First you create a directory for this particular build/configuration.

```
$ mkdir test-build
$ cd test-build
```

Then you run cmake

```
$ cmake -DCMAKE_BUILD_TYPE=Test ../
```

### Building the tests

Once inside the build directory you can just run:

```
$ cmake --build .
```


### Running the tests
To run the tests, use the command (make sure you already built the tests before running!):

```
$ ctest
```

To see fancy colored output with test details, run:
```
$ export GTEST_COLOR=1; ctest -V
```

### Running Tests in CLion

To add the build target, to the CMake preferences (**Build,Execution,Deployment->CMake**) and add click **+** to add a new profile. Call the profile **Test**. You can also type **Test** into the build profile. Leave the toolchain as **Default** and the CMake options blank.

Select the Test configuration from the configuration dropdown. You should see an **All CTest | Test** target. If you do, you can just run that. If you don't, you can go to the **Edit Configurations** menu and add a new configuration. Add a new **CTest Application**. It should default name to **"All Tests"** and targets to **All targets**. You can now select it and run it to run all the unit tests!

If you run into a `CMake 3.18 or high is required` error, go back to CMake preferences (**Build,Execution,Deployment->Toolchains**), create a new toolchain named **bristlemouth_tests**, and only change the CMake path to the one in your conda env. Then change the **Test** CMake profile's toolchain to use **bristlemouth_tests**.

## Updating CDDL/CBOR message files

If you want to change any of the message types encoded/decoded in the Bristlemouth client library, you will need to modify the .msg files
found in ```$PROJ_BASE/tools/scripts/cddl/msgs```. You will then need to run ```cddl_code_gen.sh``` found in ```$PROJ_BASE/tools/scripts/cddl```
