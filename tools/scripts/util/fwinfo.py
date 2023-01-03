"""
Search for version information in binary files!
"""
import struct
import argparse
from collections import namedtuple
from pprint import pprint

# See version.h for more information
VersionHeader = namedtuple(
    "VersionHeader", "magic gitSHA maj min rev hwVersion flags versionStrLen"
)
magic = 0xDF7F9AFDEC06627C
header_format = "QIBBBBIH"
header_len = struct.calcsize(header_format)

flag_fields = [
    "ENG",
    "DIRTY",
    "IS_BOOTLOADER",
    "SIGNATURE_SUPPORT",
    "ENCRYPTION_SUPPORT",
]

hwVersions = {
    11:"Pluteus",
    12:"Sunflower",
}

def decode_flags(flags):
    flag_list = []
    for idx, field in enumerate(flag_fields):
        if (1 << idx) & flags:
            flag_list.append(field)
    return flag_list


def getVersionFromBin(filename, offset=0):
    version = None
    with open(filename, "rb") as binfile:
        file = binfile.read()

        for offset in range(offset, len(file) - header_len):
            header = VersionHeader._make(
                struct.unpack_from(header_format, file, offset=offset)
            )

            if magic == header.magic:
                version_str = file[
                    offset + header_len : offset + header_len + header.versionStrLen
                ].decode()

                flags = decode_flags(header.flags)

                version = {
                    "sha": f"{header.gitSHA:08X}",
                    "version": f"{header.maj}.{header.min}.{header.rev}",
                    "version_str": f"{version_str}",
                    "hwversion": f"{header.hwVersion}",
                    "flags": flags,
                }

                if header.hwVersion in hwVersions:
                    version["hwversion_str"] = hwVersions[header.hwVersion]
                else:
                    version["hwversion_str"] = "Unknown"

                break

    return version


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", help="Binary filename")

    args = parser.parse_args()


    version = getVersionFromBin(args.filename)
    if version:
        print("Found version information:")
        pprint(version)

        # If we found a bootloader, keep searching for main app as well
        if "IS_BOOTLOADER" in version["flags"]:
            version = getVersionFromBin(args.filename, offset=32768)
            if version:
                pprint(version)
    else:
        print("Version information not found!")
