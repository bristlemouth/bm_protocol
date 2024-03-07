import sys
import os
import time
import serial
from datetime import datetime

# Check if a COM port and file path were provided
if len(sys.argv) < 3:
    print("Please provide a COM port and a file path.")
    sys.exit(1)

# Get the COM port and file path from the command-line arguments
com_port = sys.argv[1]
file_path = sys.argv[2]

# Create the file if it doesn't exist
if not os.path.exists(file_path):
    open(file_path, 'w').close()

print("Logging serial data from {} to {}".format(com_port, file_path))

failed_to_connect_count = 0

while True:
    try:
        # Open the serial port
        ser = serial.Serial(com_port)
        failed_to_connect_count = 0

        # Open the log file in append mode
        with open(file_path, 'a') as file:
            # Write a message to the file indicating that the serial device was connected
            timestamp = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')
            file.write(f'{timestamp}: Connected to {com_port}\n')
            print(f'{timestamp}: Connected to {com_port}')
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
                    file.write(f'{timestamp}: Disconnected from {com_port}\n')
                    print(f'{timestamp}: Disconnected from {com_port}')
                    file.flush()
                    break

    except serial.SerialException as e:
        if failed_to_connect_count % 30 == 0:
          timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
          sys.stderr.write('{}: could not open port {}: {}\n'.format(timestamp, com_port, e))
        failed_to_connect_count += 1

    # Wait for a second before the next attempt
    time.sleep(0.5)