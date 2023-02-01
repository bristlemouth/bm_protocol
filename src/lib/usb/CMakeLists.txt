#
# tinyusb files/includes
#
set(USB_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(TINYUSB_DIR ${SRC_DIR}/third_party/tinyusb)
set(TINYUSB_FILES
    ${TINYUSB_DIR}/src/tusb.c
    ${TINYUSB_DIR}/src/common/tusb_fifo.c
    ${TINYUSB_DIR}/src/device/usbd.c
    ${TINYUSB_DIR}/src/device/usbd_control.c
    ${TINYUSB_DIR}/src/class/audio/audio_device.c
    ${TINYUSB_DIR}/src/class/cdc/cdc_device.c
    ${TINYUSB_DIR}/src/class/dfu/dfu_device.c
    ${TINYUSB_DIR}/src/class/dfu/dfu_rt_device.c
    ${TINYUSB_DIR}/src/class/hid/hid_device.c
    ${TINYUSB_DIR}/src/class/midi/midi_device.c
    ${TINYUSB_DIR}/src/class/msc/msc_device.c
    ${TINYUSB_DIR}/src/class/net/ecm_rndis_device.c
    ${TINYUSB_DIR}/src/class/net/ncm_device.c
    ${TINYUSB_DIR}/src/class/usbtmc/usbtmc_device.c
    ${TINYUSB_DIR}/src/class/video/video_device.c
    ${TINYUSB_DIR}/src/class/vendor/vendor_device.c
    ${TINYUSB_DIR}/src/portable/synopsys/dwc2/dcd_dwc2.c

    ${CMAKE_CURRENT_SOURCE_DIR}/usb.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/sd_usb_msc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.cpp

    PARENT_SCOPE)

set(TINYUSB_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TINYUSB_DIR}/src
    PARENT_SCOPE
    )

set_source_files_properties(
    ${TINYUSB_DIR}/src/tusb.c
    ${TINYUSB_DIR}/src/device/usbd.c
    ${TINYUSB_DIR}/src/portable/synopsys/dwc2/dcd_dwc2.c

    DIRECTORY ${SRC_DIR}

    PROPERTIES
    COMPILE_FLAGS "-Wno-format-nonliteral -Wno-format"
    )
