"""
____________ _____ _____ _____ _      ________  ________ _   _ _____ _   _
| ___ \ ___ \_   _/  ___|_   _| |    |  ___|  \/  |  _  | | | |_   _| | | |
| |_/ / |_/ / | | \ `--.  | | | |    | |__ | .  . | | | | | | | | | | |_| |
| ___ \    /  | |  `--. \ | | | |    |  __|| |\/| | | | | | | | | | |  _  |
| |_/ / |\ \ _| |_/\__/ / | | | |____| |___| |  | \ \_/ / |_| | | | | | | |
\____/\_| \_|\___/\____/  \_/ \_____/\____/\_|  |_/\___/ \___/  \_/ \_| |_/
"""

import os
import sys
import time
import argparse
import crcmod
from pathlib import Path
from binascii import unhexlify
import serial
from dataclasses import dataclass
from collections import namedtuple
import struct
import signal
import math
from ipaddress import ip_address, IPv6Address
import socket

class BM_VERSION:
    V0 = 0
    V1 = 1

class BM_PAYLOAD_TYPE:
    GENERIC = 0
    IEEE802154 = 1
    DFU = 2

class BM_DFU_FRM_TYPE:
    START = 0
    PAYLOAD_REQ = 1
    PAYLOAD = 2
    END = 3
    ACK = 4
    ABORT = 5
    HEARTBEAT = 6
    BEGIN_HOST = 7

class BM_DFU_RETVAL:
    FAIL = 0
    SUCCESS = 1

CHUNK_SIZE = 512

BM_DFU_FRM_HDR_SIZE = 1

magic_num = [0xDE, 0xCA, 0xFB, 0xAD]
MAGIC_NUM = 0xADFBCADE
src_addr = [0, 0, 0, 0]
dst_addr = [0, 0, 0, 0]

crc16 = crcmod.predefined.mkCrcFun("kermit")
ser = None
img_size = None
img_data = None
num_chunks = None

def validIPv6Address(IP: str) -> str:
    try:
        if type(ip_address(IP)) is IPv6Address:
            return True
    except ValueError:
        return False

def wait_for_valid_header():
    global ser

    # Wait for magic number
    while True:
        if not ser is None and ser.isOpen():
            number = struct.unpack('<I', ser.read(4))[0]
            if (number == MAGIC_NUM):
                break

        else:
            print("Could not open Serial Port")
            ser.close()
            break
    # Next get length
    payload_len = struct.unpack('<H', ser.read(2))[0]
    return payload_len

def wait_for_ack():
    global ser
    retries = 0

    while retries < 5:
        if not ser is None and ser.isOpen():
            payload_len = wait_for_valid_header()
            out = list(ser.read(payload_len))
            if out[0] == BM_DFU_FRM_TYPE.ACK and out[33] == BM_DFU_RETVAL.SUCCESS:
                return 1
            else:
                retries += 1
        else:
            print("Could not open Serial Port")
            ser.close()
            break
    return 0

def send_bm_frame(frame):
    global ser

    if not ser is None and ser.isOpen():
        ser.write(frame)
    else:
        print("Could not open Serial Port")
        ser.close()

# DFU Payload
DFU_PAYLOAD_HEADER = namedtuple(
    "DFU_PAYLOAD_HEADER",
    "magic_number size type src_addr0 src_addr1 src_addr2 src_addr3 dst_addr0 dst_addr1 dst_addr2 dst_addr3 length",
)
# https://docs.python.org/3/library/struct.html#format-characters
DFU_PAYLOAD_STRUCT = "<LHB8LH"

def send_dfu_payload(chunk_num):
    global img_size
    global img_data
    frame = []

    # Determine frame size based on chunk Number
    if chunk_num != (num_chunks - 1):
        payload_len = CHUNK_SIZE
    else:
        payload_len = img_size - (chunk_num * CHUNK_SIZE)

    frame_size = payload_len + BM_DFU_FRM_HDR_SIZE + 34

    # "new way"
    header = DFU_PAYLOAD_HEADER(
        MAGIC_NUM, 
        frame_size, 
        BM_DFU_FRM_TYPE.PAYLOAD, 
        src_addr[0],
        src_addr[1],
        src_addr[2],
        src_addr[3],
        dst_addr[0],
        dst_addr[1],
        dst_addr[2],
        dst_addr[3],
        payload_len)
    payload = struct.pack(DFU_PAYLOAD_STRUCT, *header)
    payload += bytearray(
        img_data[(chunk_num * CHUNK_SIZE) : ((chunk_num * CHUNK_SIZE) + payload_len)]
    )

    send_bm_frame(payload)

# DFU Payload
DFU_REQ_HEADER = namedtuple(
    "DFU_REQ_HEADER",
    "magic_number size type src_addr0 src_addr1 src_addr2 src_addr3 dst_addr0 dst_addr1 dst_addr2 dst_addr3 img_size chunk_size img_crc maj min",
)
# https://docs.python.org/3/library/struct.html#format-characters
DFU_REQ_STRUCT = "<LHB8LLHHBB"


def send_dfu_request(img_crc, major_ver, minor_ver):
    global img_size
    frame = []

    frame_size = 42 + BM_DFU_FRM_HDR_SIZE  # TODO: Don't hardcode this

    # "new way"
    header = DFU_REQ_HEADER(
        MAGIC_NUM,
        frame_size,
        BM_DFU_FRM_TYPE.BEGIN_HOST,
        src_addr[0],
        src_addr[1],
        src_addr[2],
        src_addr[3],
        dst_addr[0],
        dst_addr[1],
        dst_addr[2],
        dst_addr[3],
        img_size,
        CHUNK_SIZE,
        img_crc,
        int(major_ver),
        int(minor_ver),
    )
    payload = struct.pack(DFU_REQ_STRUCT, *header)

    send_bm_frame(payload)

def main(cwd, args):
    global ser
    global num_chunks
    global img_size
    global img_data
    global dst_addr

    print("Attempting to update Downstream client device")

    abs_path = cwd + "/" + args.image

    if not os.path.exists(abs_path):
        print("File does not exist")
        return

    if not os.path.exists(args.port):
        print("Port does not exist")
        return
    else:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=5,
        )
        ser.close()
        ser.open()

    if validIPv6Address(args.dst):
        dst_addr_raw = args.dst
        binaryIP = socket.inet_pton(socket.AF_INET6, dst_addr_raw);
        dst_addr[0] = int.from_bytes(binaryIP[0:4], "little")
        dst_addr[1] = int.from_bytes(binaryIP[4:8], "little")
        dst_addr[2] = int.from_bytes(binaryIP[8:12], "little")
        dst_addr[3] = int.from_bytes(binaryIP[12:16], "little")
    else:
        print("Invalid IPv6 Address")
        sys.exit(1)

    img_data = Path(abs_path).read_bytes()

    # Get image characteristics
    img_size = len(img_data)
    img_crc = crc16(img_data)
    minor_ver = args.minor_ver
    major_ver = args.major_ver

    num_chunks = math.ceil(img_size / CHUNK_SIZE)

    # Send request to Device and wait for ACK
    num_requests = 0
    while True:
        if num_requests < 3:
            send_dfu_request(img_crc, major_ver, minor_ver)

            # Wait for Acknowledgement from DFU End-Device Host
            if wait_for_ack():
                break
            else:
                num_requests += 1

        else:
            print("No ACK received after 3 attempts")
            return

    # Service Chunk Requests from BM Device
    while True:
        payload_len = wait_for_valid_header()
        out = list(ser.read(payload_len))
        if out[0] == BM_DFU_FRM_TYPE.PAYLOAD_REQ:
            # Frame Num is 2 bytes
            concat_payload_num = out[33]
            concat_payload_num |= out[34] << 8
            send_dfu_payload(concat_payload_num)
        elif out[0] == BM_DFU_FRM_TYPE.END:
            if out[33] == 1:
                print("Successful update of downstream Client")
            else:
                print("Failed to update downstream Client")
            break


def parse_args():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "-i", "--image", dest="image", required=True, help="absolute path to BM Image"
    )

    parser.add_argument(
        "-p", "--port", dest="port", required=True, help="Absolute path to Port"
    )

    parser.add_argument("-b", "--baud", dest="baud", required=True, help="Baudrate")

    parser.add_argument(
        "--ip", dest="dst", required=True, help="Destination IP Address"
    )

    parser.add_argument(
        "--major", dest="major_ver", required=True, help="Major Version of BM Image"
    )
    parser.add_argument(
        "--minor", dest="minor_ver", required=True, help="Minor Version of BM Image"
    )
    return parser.parse_args()

if __name__ == "__main__":
    cwd = os.getcwd()
    args = parse_args()
    main(cwd, args)