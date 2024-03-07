import sys
import os
import time
import serial
from datetime import datetime

# Set the COM port and baud rate
com_port = '/dev/cu.usbmodem00006A2926A71'  # replace with your COM port

# Check if a file path was provided
if len(sys.argv) < 2:
    print("Please provide a file path.")
    sys.exit(1)

# Get the file path from the command-line arguments
file_path = sys.argv[1]

# Create the file if it doesn't exist
if not os.path.exists(file_path):
    open(file_path, 'w').close()

while True:
    try:
        # Open the serial port
        ser = serial.Serial(com_port)

        # Open the log file in append mode
        with open(file_path, 'a') as file:
            # Write a message to the file indicating that the serial device was connected
            timestamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
            file.write(f'{timestamp}: Connected to {com_port}\n')
            file.flush()

            # Continuously read from the serial port and write to the log file
            while True:
                try:
                    line = ser.readline().decode(errors='replace')
                    if line:
                        timestamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
                        file.write(f'{timestamp}: {line}')
                        file.flush()
                except serial.SerialException:
                    # Write a message to the file indicating that the serial device was disconnected
                    timestamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
                    file.write(f'{timestamp}: Disconnected from {com_port}\n')
                    file.flush()
                    break

    except serial.SerialException as e:
        sys.stderr.write('could not open port {}: {}\n'.format(com_port, e))

    # Wait for a second before the next attempt
    time.sleep(1)