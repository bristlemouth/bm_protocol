import sys
import datetime

def parse_timestamp(line):
    timestamp_str = line.split("|")[0].strip()
    # Remove 'Z' if present
    if timestamp_str.endswith('Z'):
        timestamp_str = timestamp_str[:-1]
    return datetime.datetime.fromisoformat(timestamp_str)

def merge_logs(file1, file2, output_file):
    with open(file1, 'r') as f1, open(file2, 'r') as f2, open(output_file, 'w') as out:
        lines1 = f1.readlines()
        lines2 = f2.readlines()

        # Sort lines by timestamp
        sorted_lines = sorted(lines1 + lines2, key=parse_timestamp)

        for line in sorted_lines:
            out.write(line)

if len(sys.argv) != 4:
    print("Usage: python script.py <input_file1> <input_file2> <output_file>")
else:
    merge_logs(sys.argv[1], sys.argv[2], sys.argv[3])