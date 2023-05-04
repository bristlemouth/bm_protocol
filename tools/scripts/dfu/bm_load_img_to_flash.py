import argparse
import serial
import os
import crcmod
from pathlib import Path
from base64 import b64encode
from collections import namedtuple
import struct
from typing import Dict
from typing import Any

CLI_WRITE_SIZE = 128
CHUNK_SIZE = 512

# Header Payload
DFU_HEADER = namedtuple( # NOTE: Must be in sync with bm_dfu_message_structs.h
    "DFU_HEADER",
    "img_size chunk_size img_crc maj min filter_key gitSHA",
)
# https://docs.python.org/3/library/struct.html#format-characters
DFU_HEADER_STRUCT_ENCODING = "<LHHBBLL"

NVM_DFU_WRITE_CMD_STR = "nvm b64write dfu"
NVM_DFU_CRC16_CMD_STR = "nvm crc16 dfu"

def getVersionFromBin(filename:str, offset:int=0) -> Dict[str, Any]:
    VersionHeader = namedtuple(
        "VersionHeader", "magic gitSHA maj min"
    )
    magic = 0xDF7F9AFDEC06627C
    header_format = "QIBB"
    header_len = struct.calcsize(header_format)
    version = None
    with open(filename, "rb") as binfile:
        file = binfile.read()

        for offset in range(offset, len(file) - header_len):
            header = VersionHeader._make(
                struct.unpack_from(header_format, file, offset=offset)
            )

            if magic == header.magic:
                version = {
                    "sha": f"{header.gitSHA:08X}",
                    "version": f"{header.maj}.{header.min}",
                }
                break
    return version

def main(img_path:str, port:str, baud:int) -> None:
    abs_path = os.path.realpath(img_path)

    # Validity Checks / port open
    if not os.path.exists(abs_path):
        print("File does not exist")
        return
    
    if not os.path.exists(port):
        print("Port does not exist")
        return

    fw_ver = getVersionFromBin(abs_path)
    major = int(fw_ver["version"].split(".")[0])
    minor = int(fw_ver["version"].split(".")[1])
    gitSHA = int(fw_ver["sha"],16)
    ser = serial.Serial(
        port=port,
        baudrate=baud,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS,
        timeout=30,
    )

    # Get image + crc
    crc16 = crcmod.predefined.mkCrcFun("kermit")
    img_data = Path(abs_path).read_bytes()

    # Get image characteristics
    img_size = len(img_data)
    img_crc = crc16(img_data)
    print(f"Image Info: - size: {img_size}, crc16:{img_crc}, major: {major}, minor: {minor}, gitSHA: {gitSHA}")

    # send header
    header = DFU_HEADER(
        img_size,
        CHUNK_SIZE,
        img_crc,
        major,
        minor,
        0,
        gitSHA
    )
    header_payload = struct.pack(DFU_HEADER_STRUCT_ENCODING, *header)
    b64_header_payload = b64encode(header_payload).decode()
    ser.write(f"{NVM_DFU_WRITE_CMD_STR} 0 {b64_header_payload}\n".encode())
    if "#" not in ser.read_until(b'#').decode():
        print("Failed to write to flash.")
        return
    ser.flush()

    # send image 
    print("Sending image")
    bytes_to_send = img_size
    header_size = len(header_payload)
    while bytes_to_send:
        img_offset = img_size - bytes_to_send
        dest_offset = header_size + img_offset
        size_to_send = CLI_WRITE_SIZE if(bytes_to_send > CLI_WRITE_SIZE) else bytes_to_send
        bin_payload = bytearray(img_data[img_offset : img_offset+size_to_send])
        b64_payload = b64encode(bin_payload).decode()
        print(f"Writing offset {img_offset} out of {img_size}: {b64_payload}")
        ser.write(f"{NVM_DFU_WRITE_CMD_STR} {dest_offset} {b64_payload}\n".encode())
        if "#" not in ser.read_until(b'#').decode():
            print("Failed to write to flash.")
            return
        ser.flush()
        bytes_to_send -= size_to_send

    # verify image 
    print("Verifying Image")
    ser.write(f"{NVM_DFU_CRC16_CMD_STR} {header_size} {img_size}\n".encode())
    result_bytes = ser.read_until(b'#')
    ser.flush()
    if not result_bytes:
        print("Unable to read crc16 from serial")
        return
    
    result_str = result_bytes.decode()
    comp_crc16 = int(result_str.split("<crc16>")[1].split('#')[0],16)
    if comp_crc16 == img_crc:
        print("Validation success! Image loaded in flash!")
    else:
        print(f"Validation failed crc16: {img_crc} computed crc16: {comp_crc16}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "-i", "--image", dest="image", required=True, help="absolute path to BM Image"
    )
    parser.add_argument(
        "-p", "--port", dest="port", required=True, help="Absolute path to Port"
    )
    parser.add_argument(
        "-b", "--baud", dest="baud", required=False, help="Baudrate", default=921600
    )
    args = parser.parse_args()
    try:
        main(args.image, args.port, args.baud)
    except Exception as e:
        print("Failed to load image.")
        print(str(e))
