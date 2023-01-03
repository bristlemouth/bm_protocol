from collections import OrderedDict, namedtuple
from os.path import isfile, join
from pprint import pprint
import argparse
import git
import os
import pickle
import re
import subprocess
import sys
import tempfile
import yaml


# Extract enums from filename
def get_enums(filename):
    enums = {}
    current_enum = {}
    description = None
    with open(filename, "r") as source_file:
        lines = source_file.readlines()
        enum_started = False
        val = 0

        for line in lines:
            if re.match(r"^\s?(?:typedef )?enum\s?{\s?", line):
                enum_started = True
                continue

            end_match = re.match(r"^\s?}\s?(?P<name>[A-Za-z0-9_]+)\s?;", line)
            if enum_started and end_match:
                enum_started = False

                enums[end_match.group("name")] = current_enum
                current_enum = {}
                continue

            if re.match(r"^\s+\/\/", line):
                description = line.split("//")[1].strip()
                continue

            if enum_started:
                match = re.match(r"^\s*(?P<name>[A-Za-z0-9_]+)(?:\s?=\s?)?(?P<value>[0-9]+)?\s?,\s?", line)

                # Skip empty lines
                if match is None:
                    continue

                name = match.group("name")
                if match.group("value"):
                    val = int(match.group("value"))

                current_enum[val] = name

                val += 1

    return enums

parser = argparse.ArgumentParser()
parser.add_argument("filename", help="filename to parse")
args = parser.parse_args()
pprint(get_enums(args.filename))

