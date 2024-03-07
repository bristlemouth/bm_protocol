import sys
import os
import time
import serial
from datetime import datetime
from serial.tools import list_ports

def connect_to_bristlemouth():
    for port in list_ports.comports():
        if 'Bristlemouth' in port.description:
            try:
                ser = serial.Serial(port.device)
                print(f'Connected to {port.device}')
                return ser
            except serial.SerialException as e:
                print(f'Could not open port {port.device}: {e}')
    return None

# Check if a COM port and file path were provided
if len(sys.argv) < 2:
    print("Please provide a COM port and a file path.")
    sys.exit(1)

# Get the file path from the command-line arguments
file_path = sys.argv[1]

# Create the file if it doesn't exist
if not os.path.exists(file_path):
    open(file_path, 'w').close()

failed_to_connect_count = 0

while True:
    try:
        # Open the serial port
        ser = connect_to_bristlemouth()
        if ser is None:
            raise serial.SerialException
        failed_to_connect_count = 0
        print("Logging serial data from {} to {}".format(ser.port, file_path))

        # Open the log file in append mode
        with open(file_path, 'a') as file:
            # Write a message to the file indicating that the serial device was connected
            timestamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
            file.write(f'{timestamp}: Connected to {ser.port}\n')
            print(f'{timestamp}: Connected to {ser.port}')
            file.flush()

            # Continuously read from the serial port and write to the log file
            while True:
                try:
                    line = ser.readline().decode(errors='replace')
                    if line:
                        if "rtc: 0," in line or "rtc: ," in line:
                            print("RTC 0 detected!!!")
                        timestamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
                        file.write(f'{timestamp}: {line}')
                        file.flush()
                except serial.SerialException:
                    # Write a message to the file indicating that the serial device was disconnected
                    timestamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
                    file.write(f'{timestamp}: Disconnected from {ser.port}\n')
                    print(f'{timestamp}: Disconnected from {ser.port}')
                    file.flush()
                    break

    except serial.SerialException as e:
        if failed_to_connect_count % 30 == 0:
          timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
          sys.stderr.write('{}: lost/could not find a bristlmouth port\n'.format(timestamp))
        failed_to_connect_count += 1

    # Wait for a second before the next attempt
    time.sleep(0.5)