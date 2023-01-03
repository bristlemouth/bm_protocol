import argparse
import shlex
import struct
import sys
from collections import namedtuple

if sys.version_info.major > 2:
    from builtins import range


class Queue(gdb.Command):
    """Print out FreeRTOS Queue
    """

    def __init__(self):
        super(Queue, self).__init__("queue", gdb.COMMAND_USER)

    def invoke(self, args, from_tty):
        # Use argparse so we can call `trace --verbose` or
        # `trace --filename somefile.txt` within GDB
        parser = argparse.ArgumentParser()
        parser.add_argument("addr", help="queue name/address")
        parser.add_argument("--verbose", action="store_true")
        args = parser.parse_args(shlex.split(args))


# Do the thing!
Queue()
