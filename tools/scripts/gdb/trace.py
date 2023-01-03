import argparse
import shlex
import struct
import sys
from collections import namedtuple

if sys.version_info.major > 2:
    from builtins import range

# Used for reading struct from memory
eventStruct = namedtuple("event", "timestamp eventType arg")


class Trace(gdb.Command):
    """Prints the captured trace
    Usage: in gdb `source /path/to/trace.py`
    `trace` or `trace verbose`
    """

    def _loadTaskInfo(self, tcbAddr):
        if tcbAddr not in self.tcbs:
            TCB_t = gdb.lookup_type("TCB_t")
            task = gdb.Value(tcbAddr).cast(TCB_t.pointer()).dereference()
            self.tcbs[tcbAddr] = task["pcTaskName"].string()

    def _getTaskName(self, tcbAddr):
        if tcbAddr not in self.tcbs:
            self._loadTaskInfo(tcbAddr)

        return self.tcbs[tcbAddr]

    def _getFunctionName(self, addr):
        if addr not in self.functions:
            block = gdb.block_for_pc(addr)
            self.functions[addr] = block.function.print_name

        return self.functions[addr]

    def _getSerialDeviceName(self, addr):
        if addr not in self.serial_devices:
            SerialHandle_t = gdb.lookup_type("SerialHandle_t")
            serialHandle = gdb.Value(addr).cast(SerialHandle_t.pointer()).dereference()
            self.serial_devices[addr] = str(serialHandle["name"].string())

        return self.serial_devices[addr]

    def _processTaskStart(self, event, verbose=False):
        self.taskStartTime = event["timestamp"]
        self.taskRunning = event["arg"]
        self._loadTaskInfo(event["arg"])

        if verbose:
            self._print(
                "{:010d}: {} Task Started ({} since last event)".format(
                    event["timestamp"],
                    self._getTaskName(event["arg"]),
                    (int(event["timestamp"]) - self.lastEventTime),
                )
            )

    def _processTaskStop(self, event, verbose=False):

        if self.taskStartTime is not None:
            duration = int(event["timestamp"]) - self.taskStartTime
        else:
            duration = "N/A"

        if verbose:
            self._print(
                "{:010d}: {} Task Finished - Duration: {}".format(
                    event["timestamp"], self._getTaskName(event["arg"]), duration
                )
            )
        else:
            if self.taskRunning:
                taskName = self._getTaskName(self.taskRunning)
            else:
                taskName = self._getTaskName(event["arg"])
            self._print("{: >30}: {}".format(taskName, duration))

        # Clear task tracking variables
        self.taskStartTime = None
        self.taskRunning = None

    def _processISRStart(self, event, verbose=False):
        fnAddr = event["arg"]
        fnName = self._getFunctionName(fnAddr)
        self.isrStartTimes[fnName] = event["timestamp"]

        if verbose:
            self._print(
                "{:010d}: {} Started ({} since last event)".format(
                    event["timestamp"],
                    fnName,
                    (int(event["timestamp"]) - self.lastEventTime),
                )
            )

    def _processISRStop(self, event, verbose=False):
        fnAddr = event["arg"]
        fnName = self._getFunctionName(fnAddr)

        if fnName not in self.isrStartTimes or self.isrStartTimes[fnName] is None:
            duration = "N/A"
        else:
            duration = int(event["timestamp"]) - self.isrStartTimes[fnName]

        if verbose:
            self._print(
                "{:010d}: {} Finished - Duration: {}".format(
                    event["timestamp"], fnName, duration
                )
            )
        else:
            self._print("{: >30}: {}".format(fnName, duration))
            self.isrStartTimes[fnName] = None

    def _processSDStart(self, event, rw, verbose=False):
        self.sdSector = event["arg"]
        self.sdStart = event["timestamp"]

        if verbose:
            self._print(
                "{:010d}: SD {} Started [sector {:08X}] ({} since last event)".format(
                    event["timestamp"],
                    rw,
                    self.sdSector,
                    (int(event["timestamp"]) - self.lastEventTime),
                )
            )

    def _processSDStop(self, event, rw, err=False, verbose=False):

        if self.sdStart:
            duration = event["timestamp"] - self.sdStart
        else:
            duration = "N/A"

        if err:
            errStr = "[ERR] "
        else:
            errStr = ""

        if verbose:
            self._print(
                "{:010d}: SD {} {}Finished - Duration {} [count {}]".format(
                    event["timestamp"], rw, errStr, duration, event["arg"]
                )
            )
        else:
            actionStr = "SD {}".format(rw)
            self._print(
                "{: >30}: {} [Sector: {:08X} Count: {}]".format(
                    actionStr, duration, self.sdSector, event["arg"]
                )
            )

        self.sdSector = 0xFFFFFFFF
        self.sdStart = None

    def _processSDReadStart(self, event, verbose=False):
        self._processSDStart(event, "Read", verbose)

    def _processSDReadStop(self, event, verbose=False):
        self._processSDStop(event, "Read", False, verbose)

    def _processSDReadErr(self, event, verbose=False):
        self._processSDStop(event, "Read", True, verbose)

    def _processSDWriteStart(self, event, verbose=False):
        self._processSDStart(event, "Write", verbose)

    def _processSDWriteStop(self, event, verbose=False):
        self._processSDStop(event, "Write", False, verbose)

    def _processSDWriteErr(self, event, verbose=False):
        self._processSDStop(event, "Write", True, verbose)

    def _processSerialByte(self, event, verbose=False):

        handle_raw = event["arg"] & 0xFFFF
        handle = self._getSerialDeviceName(handle_raw + 0x20000000)

        byte = (event["arg"] >> 24) & 0xFF
        if byte == 0x0A:
            byte_str = "\\n"
        elif byte == 0x0D:
            byte_str = "\\r"
        elif byte > 0x20 and byte < 0x7E:
            byte_str = chr(byte)
        else:
            byte_str = ""

        if (event["arg"] >> 16) & 1:
            direction = "TX"
        else:
            direction = "RX"

        if (event["arg"] >> 17) & 1:
            isr = "ISR"
        else:
            isr = ""

        self._print(
            "{:>10} {} {:>3} {:02X} {}".format(handle, direction, isr, byte, byte_str)
        )

    def _processEvent(self, event, verbose=False):
        evt = {
            "eventType": str(event["eventType"]),
            "timestamp": int(event["timestamp"]),
            "arg": int(event["arg"]),
        }

        if self.lastEventTime is None:
            self.lastEventTime = int(event["timestamp"])

        if evt["eventType"] in self.eventDecoders:
            self.eventDecoders[evt["eventType"]](evt, verbose)
        else:
            self._print(evt["eventType"])

        # Update last event time
        self.lastEventTime = int(event["timestamp"])

    def __init__(self):
        super(Trace, self).__init__("trace", gdb.COMMAND_USER)

        # Register decoders
        self.eventDecoders = {}
        self.eventDecoders["kTraceEventTaskStart"] = self._processTaskStart
        self.eventDecoders["kTraceEventTaskStop"] = self._processTaskStop
        self.eventDecoders["kTraceEventISRStart"] = self._processISRStart
        self.eventDecoders["kTraceEventISRStop"] = self._processISRStop
        self.eventDecoders["kTraceEventSDReadStart"] = self._processSDReadStart
        self.eventDecoders["kTraceEventSDReadStop"] = self._processSDReadStop
        self.eventDecoders["kTraceEventSDReadErr"] = self._processSDReadErr
        self.eventDecoders["kTraceEventSDWriteStart"] = self._processSDWriteStart
        self.eventDecoders["kTraceEventSDWriteStop"] = self._processSDWriteStop
        self.eventDecoders["kTraceEventSDWriteErr"] = self._processSDWriteErr
        self.eventDecoders["kTraceEventSerialByte"] = self._processSerialByte

    def _getEventFromBuff(self, buff, idx):
        eventDict = eventStruct._asdict(
            eventStruct._make(
                struct.unpack_from(
                    self.eventStructFormat, buff, idx * self.eventStructSize
                )
            )
        )

        eventDict["eventType"] = self.traceEventTypes[eventDict["eventType"]]

        return eventDict

    def _process_trace(self, verbose=False):
        try:
            traceEvent_t = gdb.lookup_type("traceEvent_t")
            print("Loading traceEvents")
            traceEvents = gdb.parse_and_eval("traceEvents")
        except gdb.error as err:
            print(err)
            print("ERROR: Firmware was not compiled with trace support!")
            return

        traceEventSize = int(traceEvents.type.sizeof / traceEvent_t.sizeof)
        traceEventIdx = int(gdb.parse_and_eval("traceEventIdx"))

        if str(traceEvents[traceEventSize - 1]["eventType"]) == "kTraceEventUnknown":
            indices = list(range(0, traceEventIdx))
            self._print("total traceEvents: " + str(traceEventIdx))
        else:
            # Create index list wrapping around circular buffer
            indices = list(range(traceEventIdx, traceEventSize)) + list(
                range(0, traceEventIdx)
            )
            self._print("total traceEvents: " + str(traceEventSize))

        self._print("traceEventIdx: " + str(traceEventIdx))

        start_time = int(traceEvents[indices[0]]["timestamp"])
        end_time = int(traceEvents[indices[-1]]["timestamp"])

        # The function traceEnable is only defined when using the ARM coreDebug clock
        traceEnable, _ = gdb.lookup_symbol("traceEnable")

        # Default to 1kHz ticks
        traceClockHz = 1000

        # If we're using CoreDebug clock, use system clock speed
        if traceEnable:
            traceClockHz = float(gdb.parse_and_eval("SystemCoreClock"))

        self._print(
            "trace duration: {:0.6f} s".format(
                ((end_time - start_time) & 0xFFFFFFFF) / traceClockHz,
            )
        )

        print(
            "Reading traceEvents buffer ({} bytes)...".format(
                int(traceEvents.type.sizeof)
            )
        )

        # Read entire traceEventsBuffer at once in a single transaction
        # This is WAY faster than reading each item individually
        traceEventsBuff = (
            gdb.inferiors()[0]
            .read_memory(int(traceEvents.address), int(traceEvents.type.sizeof))
            .tobytes()
        )
        # traceEventsBuff = gdb.inferiors()[0].read_memory(int(traceEvents.address),64).tobytes()
        print("Done reading buffer")

        for idx in indices:
            # This is the old "slow" way of processing events, lots of GDB transactions
            # but is agnostic to struct formatting
            # self._processEvent(traceEvents[i], verbose)

            # This is the fast way! (It does require that the decoding struct is correct)
            # Need to update if TRACE_FAST is defined
            self._processEvent(self._getEventFromBuff(traceEventsBuff, idx), verbose)

    # Print to console or file, depending on configuration
    def _print(self, *args):
        if self.file:
            self.file.write("".join(args) + "\n")
        else:
            print(*args)

    def invoke(self, args, from_tty):
        self.taskRunning = None
        self.taskStartTime = None
        self.sdSector = 0xFFFFFFFF
        self.sdStart = None
        self.tcbs = {}
        self.functions = {}
        self.serial_devices = {}
        self.isrStartTimes = {}
        self.lastEventTime = None
        self.eventStructFormat = "<LBL"  # Change this accordingly if using TRACE_FAST
        self.eventStructSize = struct.calcsize(self.eventStructFormat)

        # Get event types from enum
        eventTypes = gdb.types.make_enum_dict(gdb.lookup_type("enum traceEventType"))

        # Swap key/values since make_enum_dict returns "enumStr":val
        self.traceEventTypes = dict((v, k) for k, v in eventTypes.items())

        # Use argparse so we can call `trace --verbose` or
        # `trace --filename somefile.txt` within GDB
        parser = argparse.ArgumentParser()
        parser.add_argument("--verbose", action="store_true")
        parser.add_argument("--filename", help="save trace to file")
        args = parser.parse_args(shlex.split(args))

        self.file = None

        if args.filename:
            self.file = open(args.filename, "w")
            print(f"Writing trace to {args.filename}")

        self._process_trace(args.verbose)

        if self.file:
            self.file.close()
            print(f"{args.filename} closed")


# Do the thing!
Trace()
