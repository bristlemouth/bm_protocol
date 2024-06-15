import sys
import matplotlib.pyplot as plt
import datetime

if len(sys.argv) != 2:
    print("Usage: python script.py <log_file_path>")
    sys.exit(1)

log_file_path = sys.argv[1]

# Parse log file and extract timestamps of "bm port0 up" lines
timestamps = []
with open(log_file_path, 'r') as f:
    for line in f:
        if 'bm port1 up' in line:
            timestamp_str = line.split("|")[0].strip()
            if timestamp_str.endswith('Z'):
                timestamp_str = timestamp_str[:-1]
            try:
                timestamp = datetime.datetime.fromisoformat(timestamp_str)
                timestamps.append(timestamp)
            except ValueError:
                # Ignore lines with invalid timestamps
                continue

# Check if any timestamps were found
if not timestamps:
    print(f"No 'bm port1 up' lines found in {log_file_path}")
    sys.exit(1)

# Calculate differences between consecutive timestamps
time_diffs = [0]  # First timestamp has no previous timestamp to compare with
for i in range(1, len(timestamps)):
    diff = (timestamps[i] - timestamps[i-1]).total_seconds()
    time_diffs.append(diff)

# Create a plot
plt.plot(timestamps, time_diffs, 'o-')
plt.xlabel('Time')
plt.ylabel('Time difference (seconds)')
plt.title('Time difference between consecutive "bm port1 up" occurrences')
plt.show()