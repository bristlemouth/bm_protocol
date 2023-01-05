# Bootloader Application
This application is using [MCUBoot](https://www.mcuboot.com/) to be the main bootloader for Spotter.

It allows us to verify signed images, optionally decrypt encrypted ones, before booting. It also allows us to swap a new image, while keeping the old "working" one until the new one verifies it is working properly. If not, it will automatically restore the previous image on the next reboot!

For a deep dive into MCUBoot, check out Memfault's [MCUboot Walkthrough and Porting Guide](https://interrupt.memfault.com/blog/mcuboot-overview).

# Spotter Developer Guide
## Summary
In order to work with a bootloader, the following things need to happen:
1. The bootloader itself needs to be flashed at the start of the program memory
2. The main application must be compiled and linked knowing that it will no longer be placed at the start of the program memory.
3. The main application must be signed (and encrypted, if necessary) before being flashed into the device

## Instructions
### Building the Bootloader App
**TODO: Download from github releases**

1. As usual, create a cmake build directory, for example `cmake-build/bootloader`.
2. Run `cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi-gcc.cmake -DBSP=bootloader -DAPP=bootloader -DCMAKE_BUILD_TYPE=Debug`
3. Run `make -j` to build
4. To flash the bootloader, run `make flash`

### Building the Main App (With Bootloader support)
1. As usual, create a cmake build directory, for example `cmake-build/spotter_w_bootloader`
2. Run the usual cmake command, but with the added `-DUSE_BOOTLOADER=1`, for example: `cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi-gcc.cmake -DBSP=pluteus_v3.1 -DAPP=spotter -DCMAKE_BUILD_TYPE=Debug -DUSE_BOOTLOADER=1`
3. Run `make -j` to build _and_ create the MCUBoot image (ends in .dfu.bin)
4. `make flash` will upload the image into the right spot in memory :D

### Debugging Apps with Bootloader Support
As long as the previous steps were followed, running `make debug` will work. Note that if the bootloader is currently running, gdb won't know what the functions/symbols are. You can also run `make debug` from the bootloader cmake directory to debug the bootloader itself.

### Building Combined Image
The generated **.bin** files are either the bootloader OR the main application. It is possible to generate a combined image that contains **both** the bootloader and the main application. This will be useful when using dfu-util to upgrade a non-bootloader sytem to one with bootloader support while also installing the main application.

In order to do this, we first need a previously built bootloader **.elf** file. We then need to pass the path to the bootloader elf to CMAKE like this: `cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi-gcc.cmake -DBSP=pluteus_v3.1 -DAPP=spotter -DCMAKE_BUILD_TYPE=Debug -DUSE_BOOTLOADER=1 -DBOOTLOADER_PATH=/path/to/bootloader.elf`

Once cmake has the **BOOTLOADER_PATH**, running `make -j` normally will also produce a **.unified.bin** image.

### SD Card Firmware Updates!
If you have a .dfu.bin firmware image, you can put it on the SD card and then run (over usb-serial): `update <path to image.dfu.bin>`

This will place the image in the SD card on the secondary slot in program memory. If successful, the device will reboot, and the bootloader will swap the new image with the previous one (Usually takes \~30 seconds). Once the new image has booted, you must connect over usb and run `update confirm` to ensure the new firmware image stays. Otherwise, it will revert to the previous one on the next reboot.

## Sneaky USB DFU Mode!
The bootloader image also has a feature that will enter the USB ROM bootloader if there is a magnet present on boot. To enter DFU mode, hold a magnet to the Spotter while power switch is OFF and then plug in USB.

# Encryption/Signing

MCUBoot supports signed and/or encrypted images. In order to compile with image signing support, add `-DSIGN_IMAGES=1` to the cmake command. Use `-DENCRYPT_IMAGES=1` to also enable encrypted images. This goes for both building the bootloader _and_ building the application. A bootloader with both signed and encrypted image support will reject invalid images (unsigned, or signed with wrong signature). A valid image can be **either** encrypted or not encrypted, but **must always** be signed.

The encryption keys in this directory are ***FOR DEVELOPMENT PURPOSES ONLY***.
To re-generate signing/encryption keys, run `keygen.sh` in this directory.

**TODO - auto-generate ed25519_pub_key.c and x25519_priv.key.c at compile time (and get .pem files from "vault" so we don't commit production keys to git repo)**

For more information on signing images, see the [imgtool documentation](https://docs.mcuboot.com/imgtool.html). For information on encrypted images, see the [Encrypted Images page](https://docs.mcuboot.com/encrypted_images.html) on the MCUBoot docs.

## Algorithm Selection
We are using the [ed25519](https://cryptobook.nakov.com/digital-signatures/eddsa-and-ed25519) [elliptic curve digital signature algorithm](https://en.wikipedia.org/wiki/EdDSA) due to the small key/signature size and performance. For encryption, MCUBoot uses AES-128, and exchanges the key using [x25519](https://en.wikipedia.org/wiki/Curve25519), which has small key size and is [widely used in industry.](https://ianix.com/pub/curve25519-deployment.html)

***TODO - Have the new image automatically confirm itself after some time***
