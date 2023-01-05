import subprocess
import sys

with open(sys.argv[1], 'r') as file:
	lines = file.readlines()
	for line in lines:
		subprocess.run(["memfault","--project-key","3weCmDl5GqjlGx5WPA9hMT20uFGBPrF5","post-chunk","--encoding","base64","--device-serial","TEST_DEVICE_1", line.strip()])