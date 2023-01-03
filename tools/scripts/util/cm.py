"""
"""
import argparse
import glob
import os
from pprint import pprint

parser = argparse.ArgumentParser()
parser.add_argument("path", help="BSP path")

args = parser.parse_args()

path = args.path

hfiles = glob.glob(f"{path}/**/*.h", recursive=True)

include_paths = []
for file in hfiles:
    inc_dir = os.path.dirname(file)
    if inc_dir not in include_paths:
        include_paths.append(inc_dir)


source_files = []

cfiles = glob.glob(f"{path}/**/*.c", recursive=True)
for file in cfiles:
    source_files.append(file)

cppfiles = glob.glob(f"{path}/**/*.cpp", recursive=True)
for file in cppfiles:
    source_files.append(file)

sfiles = glob.glob(f"{path}/**/*.s", recursive=True)
for file in sfiles:
    source_files.append(file)

print(
    """
set(BSP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(COMMON_BSP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)"""
)

print("set(BSP_FILES")
for file in sorted(source_files):
    print(f"        ${{CMAKE_CURRENT_SOURCE_DIR}}/{file}")
print("        PARENT_SCOPE)")

print("""set(BSP_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}""")
for inc_path in sorted(include_paths):
    print(f"        ${{CMAKE_CURRENT_SOURCE_DIR}}/{inc_path}")
print("        PARENT_SCOPE)")