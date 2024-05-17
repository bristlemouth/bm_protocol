import argparse
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.descriptions import describe_reloc_type
from elftools.dwarf.descriptions import describe_attr_value
from elftools.dwarf.dwarfinfo import DWARFInfo
# from elftools.dwarf.constants import DWARF_TAGS, DW_AT_name, DW_AT_decl_file, DW_AT_decl_line
from elftools.elf.sections import SymbolTableSection
import subprocess
import serial

def print_symbol_table(elf_file_path):
    with open(elf_file_path, 'rb') as f:
        elffile = ELFFile(f)

        for section in elffile.iter_sections():
            if isinstance(section, SymbolTableSection):
                print(f'Symbol table {section.name}:')
                for symbol in section.iter_symbols():
                    print(f'    {symbol.name}: {hex(symbol.entry.st_value)}')
    return None

def get_function_name_from_address(elf_file_path, address):
    with open(elf_file_path, 'rb') as f:
        elffile = ELFFile(f)
        for section in elffile.iter_sections():
            if isinstance(section, SymbolTableSection):
                for symbol in section.iter_symbols():
                    if symbol.entry.st_value == address:
                        return symbol.name
    return None

def get_function_location(elf_file, address):
    command = ['arm-none-eabi-addr2line', '-pCf', '-e', elf_file, str(hex(address))]
    result = subprocess.run(command, capture_output=True, text=True)
    return result.stdout.strip()

def trace_time_parsing(trace_file):
    start_times = []
    end_times = []
    print("\n\033[92mTrace buffer parsed for LPUART\033[0m")
    first_trace_buffer = True

    with open(trace_file, 'r') as f:
        for line in f:
            if "Trace buffer" in line:
                if first_trace_buffer:
                    first_trace_buffer = False
                    continue
                print(f"# of Start times {len(start_times)}: ")
                print(f"# of End times {len(end_times)}: ")
                print(f"Diff: {len(start_times) - len(end_times)}")
                counter = 0
                i = 0
                j = 0
                while i < len(start_times) and j < len(end_times):
                    start = start_times[i]
                    end = end_times[j]
                    if (end - start)/160000 > 0.09:
                        print(f"\033[91m{counter} BIG DIFF LPUART_IRQHandler start[{i}]: {start} end[{j}]: {end} execution time: {end - start} CPU cycles, or {(end - start)/160000} ms\033[0m")
                    elif (end - start) < 0:
                        while (end - start) < 0  and j < len(end_times) - 1:
                            j += 1
                            end = end_times[j]
                    else:
                        print(f"{counter} LPUART_IRQHandler start[{i}]: {start} end[{j}]: {end} execution time: {end - start} CPU cycles, or {(end - start)/160000} ms")
                    i += 1
                    j += 1
                start_times = []
                end_times = []
                print("\n\033[92mTrace buffer parsed for LPUART\033[0m")
            if "0x8019d41" in line or "0x8019d51" in line:
                if "Event type: 3" in line:
                    print("LPUART_IRQHandler start time: ", line.split("timestamp:")[1].split(",")[0])
                    start_times.append(int(line.split("timestamp:")[1].split(",")[0]))
                if "Event type: 4" in line:
                    print("LPUART_IRQHandler end time: ", line.split("timestamp:")[1].split(",")[0])
                    end_times.append(int(line.split("timestamp:")[1].split(",")[0]))
    print(f"# of Start times {len(start_times)}: ")
    print(f"# of End times {len(end_times)}: ")
    print(f"Diff: {len(start_times) - len(end_times)}")
    counter = 0
    i = 0
    j = 0
    while i < len(start_times) and j < len(end_times):
        start = start_times[i]
        end = end_times[j]
        if (end - start)/160000 > 0.09:
            print(f"\033[91m{counter} BIG DIFF LPUART_IRQHandler start[{i}]: {start} end[{j}]: {end} execution time: {end - start} CPU cycles, or {(end - start)/160000} ms\033[0m")
        elif (end - start) < 0:
            while (end - start) < 0 and j < len(end_times) - 1:
                j += 1
                end = end_times[j]
            print("\033[94mSKIPPING SINCE WE LIKELY MISSED SOME LOGS HERE\033[0m")
        else:
            print(f"{counter} LPUART_IRQHandler start[{i}]: {start} end[{j}]: {end} execution time: {end - start} CPU cycles, or {(end - start)/160000} ms")
        i += 1
        j += 1

def trace_live_parsing():
    start_times = []
    end_times = []
    ser = serial.Serial('/dev/cu.usbmodem11301')
    reset_reason_line = None
    while True:
        line = ser.readline().decode(errors='replace').strip()
        if "Reset Reason" in line:
            print(line)
            reset_reason_line = line
        if "Event type: " in line:
            print(line)
        if "End of trace buffer" in line:
            print("\n\033[92mTrace buffer parsed for LPUART\033[0m")
            if reset_reason_line:
                print(reset_reason_line)
            print(f"# of Start times {len(start_times)}: ")
            print(f"# of End times {len(end_times)}: ")
            print(f"Diff: {len(start_times) - len(end_times)}")
            counter = 0
            i = 0
            j = 0
            while i < len(start_times) and j < len(end_times):
                start = start_times[i]
                end = end_times[j]
                if (end - start)/160000 > 0.09:
                    print(f"\033[91m{counter} BIG DIFF LPUART_IRQHandler start[{i}]: {start} end[{j}]: {end} execution time: {end - start} CPU cycles, or {(end - start)/160000} ms\033[0m")
                elif (end - start) < 0:
                    while (end - start) < 0  and j < len(end_times) - 1:
                        j += 1
                        end = end_times[j]
                else:
                    print(f"{counter} LPUART_IRQHandler start[{i}]: {start} end[{j}]: {end} execution time: {end - start} CPU cycles, or {(end - start)/160000} ms")
                i += 1
                j += 1
            start_times = []
            end_times = []
            print("\n\033[92mEnd of trace buffer parsed for LPUART\033[0m")
        if "0x8019d41" in line or "0x8019d51" in line or "0x8019d61" in line:
            if "Event type: 3" in line:
                print("LPUART_IRQHandler start time: ", line.split("timestamp:")[1].split(",")[0])
                start_times.append(int(line.split("timestamp:")[1].split(",")[0]))
            if "Event type: 4" in line:
                print("LPUART_IRQHandler end time: ", line.split("timestamp:")[1].split(",")[0])
                end_times.append(int(line.split("timestamp:")[1].split(",")[0]))


# Parse command-line arguments
parser = argparse.ArgumentParser(description='Get line from address in ELF file.')
parser.add_argument('--elf_file', help='Path to the ELF file')
parser.add_argument('--address', type=lambda x: int(x, 0), help='Address to look up (in hexadecimal, e.g., 0x400800)')
parser.add_argument('--symbol-table', action='store_true', help='Print the symbol table')
parser.add_argument('--trace-file', help='Path to the trace file')
parser.add_argument('--trace-live', action='store_true', help='Live trace parsing')
args = parser.parse_args()

# Use the function
if args.symbol_table:
    print_symbol_table(args.elf_file)
# if args.address:
#     symbol_name = get_function_name_from_address(args.elf_file, args.address)
#     print(f'Function name at address {hex(args.address)}: {symbol_name}')

if args.address:
    location = get_function_location(args.elf_file, args.address)
    symbol_name = get_function_name_from_address(args.elf_file, args.address)
    if location:
        print(f'{hex(args.address)}: {location}')
    else:
        print(f'No function found at address {hex(args.address)}')

if args.trace_file:
    trace_time_parsing(args.trace_file)

if args.trace_live:
    trace_live_parsing()