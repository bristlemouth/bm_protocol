"""
Stream audio data over USB port (from bristlemouth!)

To run this script on MacOS, you must do the following:
* conda install portaudio
* pip3 install pyaudio
"""
import pyaudio
import serial
import wave
import sys
import argparse
from collections import namedtuple
import struct
import signal

sampling = True
def handler(signum, frame):
    global wavfile
    global stream
    print("Exiting")
    if wavfile:
        wavfile.close()

    stream.stop_stream()
    stream.close()

    sys.exit(0)

signal.signal(signal.SIGINT, handler)


AUDIO_HDR = namedtuple("AUDIO_HDR", "magic sample_rate num_samples sample_size flags")
AUDIO_STRUCT = "<LLHBB"

parser = argparse.ArgumentParser()
parser.add_argument("device", help="Serial port")
parser.add_argument("--filename", help="Save stream to wav file")
args, unknownargs = parser.parse_known_args()

ser = serial.Serial(args.device, 921600)

ser.flushInput()

print("Waiting for data")

# Wait for magic number
header_bytes = bytes([0x55, 0xB0, 0x10, 0xAD])

ser.read_until(header_bytes)

# Get the first header
header_bytes += ser.read(struct.calcsize(AUDIO_STRUCT)-4)
header = AUDIO_HDR._make(struct.unpack(AUDIO_STRUCT, header_bytes))

print("Starting stream!")

p = pyaudio.PyAudio()
stream = p.open(format=2,
                channels=1,
                rate=int(header.sample_rate/2),
                output=True)

wavfile = None
if args.filename:
    wavfile = wave.open(args.filename, "w")
    wavfile.setnchannels(1)
    wavfile.setsampwidth(header.sample_size)
    wavfile.setframerate(header.sample_rate)

last_num_samples = header.num_samples
while True:
    samples = ser.read(header.num_samples * header.sample_size)
    stream.write(samples)
    if wavfile:
        wavfile.writeframes(samples)

    if header.num_samples < last_num_samples:
        print("Done")
        break

    # print(".")
    # Read next header
    header_bytes = ser.read(struct.calcsize(AUDIO_STRUCT))
    header = AUDIO_HDR._make(struct.unpack(AUDIO_STRUCT, header_bytes))

if wavfile:
    wavfile.close()

    stream.stop_stream()
    stream.close()
