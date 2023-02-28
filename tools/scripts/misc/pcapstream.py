"""
Pcap capture/stream helper

Connect to the second bristlemouth USB port to start capture.
To use wireshark, make sure it is installed. You can override the default macOS
path with the --wireshark_path argument.

Use --wireshark to open a wireshark window and stream the packets

Use --filename to save to a .pcap file for later analysis
"""
from collections import namedtuple
import argparse
import os
import serial
import signal
import struct
import subprocess
import sys
import tempfile
import time

# Pcap header definition
PCAP_HDR = namedtuple(
    "PCAP_HDR",
    "magic_number version_major version_minor thiszone sigfigs snaplen network",
)
PCAP_STRUCT = "<LHHlLLL"

# Pcap record header
RECORD_HDR = namedtuple("RECORD_HDR", "ts_sec ts_usec incl_len orig_len")
RECORD_STRUCT = "<LLLL"

# Default packet header as suggested in
# https://wiki.wireshark.org/Development/LibpcapFileFormat
header = PCAP_HDR(
    magic_number=0xA1B2C3D4,
    version_major=2,
    version_minor=4,
    thiszone=0,
    sigfigs=0,
    snaplen=65535,
    network=1,
)


#
# Write bytes to pcap file and/or pipe
#
def write(data, pcap, pcap_pipe):
    if pcap_pipe:
        pcap_pipe.write(data)
        pcap_pipe.flush()

    if pcap:
        pcap.write(data)
        pcap.flush()

#
# Capture pcap data from USB serial interface
#
def capture_packets(pcap, pcap_pipe):
    header = ser.read(24)
    print(PCAP_HDR._make(struct.unpack(PCAP_STRUCT, header)))
    # TODO - check header

    write(header, pcap, pcap_pipe)

    while True:
        header = ser.read(16)
        record = RECORD_HDR._make(struct.unpack(RECORD_STRUCT, header))
        print(record)
        data = ser.read(record.incl_len)
        write(header, pcap, pcap_pipe)
        write(data, pcap, pcap_pipe)


def handler(signum, frame):
    print("Exiting")
    if pcap:
        pcap.close()
    exit(0)


signal.signal(signal.SIGINT, handler)


parser = argparse.ArgumentParser()
parser.add_argument("device", help="Serial port")
parser.add_argument(
    "--wireshark", action="store_true", help="Stream capture to wireshark"
)
parser.add_argument(
    "--wireshark_path",
    default="/Applications/Wireshark.app/Contents/MacOS/Wireshark",
    help="Stream capture to wireshark",
)
parser.add_argument("--filename", help="Save stream to pcap file")
args, unknownargs = parser.parse_known_args()

pipe_path = None
if args.wireshark:
    # Create temporary directory to create named pipe for wireshark
    tmpdir = tempfile.mkdtemp()
    pipe_path = os.path.join(tmpdir, "bm_packet_pipe")

    # Create named pipe
    os.mkfifo(pipe_path)
    print("Created pipe in", pipe_path)

    print("Running Wireshark")
    subprocess.Popen([args.wireshark_path, "-i", pipe_path, "-k"])

ser = serial.Serial(args.device)

pcap = None
pcap_pipe = None

if args.filename:
    pcap = open(args.filename, "wb")
if pipe_path:
    pcap_pipe = open(pipe_path, "wb")

try:
    capture_packets(pcap, pcap_pipe)
except BrokenPipeError:
    if pipe_path:
        print("Removing", pipe_path)
        os.unlink(pipe_path)
