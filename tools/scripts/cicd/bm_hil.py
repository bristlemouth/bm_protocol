import os
import subprocess
import serial.tools.list_ports
import serial
import re
import sys
from time import sleep
from logging_helper import LoggingHelper

directory = os.path.dirname(os.path.abspath(__file__))
cwd = directory + "/../../../src/lib/bm_core/test/scripts"
sys.path.insert(1, cwd)
from serial_helper import SerialHelper

LOGGER = None
DFU_UTIL_RETRY = 3


def execute(cmd: list[str]) -> str:
    lines = ""
    try:
        with subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
            cwd=os.getcwd(),
        ) as p:
            for line in p.stdout:
                LOGGER.log(LOGGER.DEBUG, line)
                lines += line
            if p.returncode != 0 and p.returncode is not None:
                raise subprocess.CalledProcessError(p.returncode, p.args)
            return lines
    except subprocess.CalledProcessError:
        return None


def dfu_util_update(image: str, offset: int = 0):
    debug_str = "Updating Firmware Over DFU Util"
    print(debug_str)
    LOGGER.log(LOGGER.INFO, debug_str)

    ports = serial.tools.list_ports.comports()
    fail_count = 0
    offset = 0x8000000 + offset

    # Iterate updates over all nodes connected over serial
    for port, desc, __ in sorted(ports):
        if "Bristlemouth" in desc and int(port[-1]) == 1:
            retry_count = 0
            while retry_count < DFU_UTIL_RETRY:
                status = True
                ser = SerialHelper(port)
                ser.transmit_str("\nbootloader\n")
                ser.close()
                debug_str = f"DFU Update On Port {port} For File: {image}"
                print(debug_str)
                LOGGER.log(LOGGER.INFO, debug_str)
                sleep(5.0)
                output = execute(
                    [
                        "dfu-util",
                        "-a",
                        "0",
                        "-s",
                        hex(offset) + ":leave",
                        "-D",
                        image,
                    ]
                )
                find = re.search(r"File downloaded successfully", output)
                if find is None:
                    status = False
                output_str = "DFU Update Status: "
                output_str += "Success" if status is True else "Failure"
                print(output_str)
                if status is False:
                    LOGGER.log(LOGGER.ERROR, output_str)
                    fail_count += 1
                    retry_count += 1
                    print("Failure output result:\n" + output)
                    if retry_count < DFU_UTIL_RETRY:
                        debug_str = "Retrying DFU Util Update..."
                        print(debug_str)
                        LOGGER.log(LOGGER.WARNING, debug_str)
                else:
                    LOGGER.log(LOGGER.INFO, output_str)
                    break


def run_hil_tests():
    debug_str = "Running HIL Tests"
    print(debug_str)
    LOGGER.log(LOGGER.INFO, debug_str)

    # Set CWD to bm_core HIL test scripts
    os.chdir(cwd)

    ports = serial.tools.list_ports.comports()
    fail_count = 0

    # Iterate tests over all nodes connected over serial
    for port, desc, __ in sorted(ports):
        # Determine if this is a node we want to talk to over serial
        if "Bristlemouth" in desc and int(port[-1]) == 1:
            status = True
            debug_str = f"HIL Test Running On: {port}"
            print(debug_str)
            LOGGER.log(LOGGER.INFO, debug_str)
            output = execute(
                [
                    "pytest",
                    "-s",
                    "@tests_to_run.txt",
                    "--port",
                    port,
                    "--baud",
                    "921600",
                ]
            )

            # Determine if any tests have failed
            find = re.search(r"\w+ failed, \w+ passed", output)
            if find:
                status = False
            output_str = "HIL Test Status: "
            output_str += "Success" if status is True else "Failure"
            print(output_str)
            if status is False:
                LOGGER.log(LOGGER.ERROR, output_str)
                fail_count += 1
                print("Failure output result:\n" + output)
            else:
                LOGGER.log(LOGGER.INFO, output_str)


if __name__ == "__main__":
    LOGGER = LoggingHelper("cicd", "hil_test")
    if len(sys.argv) > 2:
        dfu_util_update(sys.argv[1], int(sys.argv[2], 16))
        # Let the nodes boot and finish initializing
        sleep(10.0)
    run_hil_tests()
