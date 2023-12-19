# Firmware update instructions ($VERSION)

**NOTE:** In order to be able to do remote firmware updates, we ***MUST***
install the bootloader _and_ a bootloader enabled app. See
[readme](https://github.com/bristlemouth/bm_protocol/blob/develop/src/apps/bootloader/README.md)
for more information.

For convenience, you can do the single dfu-util command that has _both_ the
bootloader and the application as explained below.


## SD Card

The easiest way to update the firmware of a Bristlemouth node that is connected
to a Sofar Ocean Spotter bouy is to use the SD card. Connect a USB cable from
your computer to the Spotter, and start a serial terminal using pyserial-miniterm
or similar. Once connected, run the command `bm info 0` to get information about
the entire Bristlemouth network.

In the output, you need to find the Node ID of the node you want to update.
Once you have it, place a copy of the `$ELF_DFU_BIN` file on the SD card,
and rename it to `update_<NODE_ID>.bin`, for example `update_2b64cf4c62a575a0.bin`.

Then insert the SD card into the Spotter and wait for about 5 minutes.
The Spotter will automatically detect the new firmware and update the node.

If desired, you can open a serial terminal to Spotter to watch the progress
of the update, but this is not necessary. When the update is complete, you
can run `bm info 0` again to verify that the node is running the new firmware.


## USB

**IMPORTANT NOTES:**

- This method MUST NOT be interrupted or the device will become unresponsive.
  It is still recoverable at that point, but more manual steps are required.
- This method requires connecting a USB cable to the Bristlemouth node, NOT Spotter.

You'll need to have [dfu-util](http://dfu-util.sourceforge.net/) installed.
On MacOS just `brew install dfu-util`, on Ubuntu just `apt-get install dfu-util`

Connect a USB cable from your computer to the Bristlemouth node and open a
serial terminal using pyserial-miniterm or similar. If the device has
already been flashed, you can enter the ROM bootloader by typing the
`bootloader` command over the USB console. Otherwise, you need to hold the BOOT
button while pressing and releasing the reset button to enter the bootloader.

You can run `dfu-util -l` in a terminal on your computer to see if the device is detected.

To flash the image, run this command in the directory that has the firmware image:
`dfu-util -d 0483:df11 -c 1 -i 0 -a 0 -s 0x8000000:leave -D $ELF_UNIFIED_BIN`

**DO NOT INTERRUPT THE DFU PROCESS!**
