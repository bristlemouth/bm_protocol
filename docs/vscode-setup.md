# VS Code Setup

VS Code is an excellent and popular development environment that you can use
for developing Bristlemouth firmware. Here are some setup instructions.

*Please follow the instructions in
[`ENV_SETUP.md`](https://github.com/bristlemouth/bm_protocol/blob/develop/ENV_SETUP.md)
and
[`BMDK_README.md`](https://github.com/bristlemouth/bm_protocol/blob/develop/src/apps/bm_devkit/BMDK_README.md)
first.*
The VS Code setup won't work until you can successfully work in the command line.

At the top level of the repo, if it doesn't exist yet, create a `.vscode` directory.
It's ignored by git because it contains settings specific to your machine.
We're going to create and edit files in there.


## 1. CMake and the conda env

- Install the CMake Tools extension from Microsoft.
- If you don't yet have the `code` command in your terminal PATH to open the VS Code app, then:
    - In VS Code, hit Cmd-Shift-P and choose "Shell Command: Install 'code' command in PATH"
    - In order to get VS Code to work in your conda environment, you'll need to **always open it from a terminal where you're already in the conda environment**. For example, open terminal, cd to the `bm_protocol` directory, `conda activate bristlemouth`, then `code .` (with the dot argument) meaning "open the current directory in VS Code". Go ahead and quit VS Code and open it this way now.


## 2. CMake Tools "Kit"

- Hit Cmd-Shift-P and choose "CMake: Scan for Kits".
- Next hit Cmd-Shift-P and choose "CMake: Select a Kit".

If you see a kit for arm-non-eabi-gcc in your conda environment, fantastic, choose it, and skip to step 3.
If you don't see it, then you need to create a kit file.

In the `.vscode` directory create a `cmake-kits.json` file with the following contents:

```json
[
    {
        "name": "Bristlemouth arm-none-eabi",
        "compilers": {
            "C": "/Users/me/miniconda3/envs/bristlemouth/bin/arm-none-eabi-gcc",
            "CXX": "/Users/me/miniconda3/envs/bristlemouth/bin/arm-none-eabi-g++"
        },
        "toolchainFile": "${workspaceFolder}/cmake/arm-none-eabi-gcc.cmake",
        "isTrusted": true
    }
]
```

You'll have to edit the compiler paths to point to the binaries in your conda environment.

After you create that file and save it, then once again:

- Hit Cmd-Shift-P and choose "CMake: Scan for Kits".
- Next hit Cmd-Shift-P and choose "CMake: Select a Kit".
- Select the "Bristlemouth arm-none-eabi" kit.


## 3. CMake configuration

In the `.vscode` directory, create a `settings.json` file or edit it if it already exists.

The contents of the settings file should be one json object, opening with `{` at the top and closing with `}` at the bottom.

This is where you'll set CMake cache entries to choose which app you want to build with VS Code. This is an example for building the Hello World app, and you can edit this file any time you want to work on a different app.

Leave any existing content in the settings file alone, and add the following keys and values to the top-level object:

```json
    "cmake.configureOnOpen": true,
    "makefile.makePath": "/Users/me/miniconda3/envs/bristlemouth/bin/make",
    "cmake.cmakePath": "/Users/me/miniconda3/envs/bristlemouth/bin/cmake",
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-gcc.cmake",
        "-DUSE_BOOTLOADER=1",
        "-DCMAKE_BUILD_TYPE=Debug"
        "-DCMAKE_APP_TYPE=BMDK",
        "-DBSP=bm_mote_v1.0",
        "-DAPP=hello_world",
    ],
    "cmake.buildArgs": [
        "-j"
    ],
```

You'll have to edit the makePath and cmakePath to point to the binaries in your conda environment.

Hit Cmd-Shift-P and choose "CMake: Delete Cache and Reconfigure".
You should reconfigure any time you edit the settings file.


## 4. Create tasks

In the `.vscode` directory, create a `tasks.json` file with the following content, or edit it if it already exists.

```json
{
	"version": "2.0.0",
	"tasks": [
		{
			"type": "shell",
			"label": "Run unit tests",
			"command": "cd test-build && make -j && ctest",
			"problemMatcher": [],
			"detail": "Run ctest in the test-build directory"
		}
	]
}
```

Run these by hitting Cmd-Shift-P and choosing "Tasks: Run Task".


## Winning!

You can now:

- Build firmware by hitting F7 or choosing "CMake: Build" from the command palette (Cmd-Shift-P)
- Run unit tests by running the "Run unit tests" task (Cmd-Shift-P "Tasks: Run Task")

**IMPORTANT NOTE:** VS Code always builds in the top level `build` folder.
When you want to flash the compiled binary to your dev kit,
you'll need to grab the correct file from `build/src`, for example,
`build/src/bm_mote_v1.0-hello_world-dbg.elf.dfu.bin`.
