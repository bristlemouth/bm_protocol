"""
Upload a firmware binary to Notehub.

Setup:
    Create a Notehub personal access token (Notehub > Settings > Personal Access
    Tokens) and export it, or put it in a .env file at the repo root:

        NOTEHUB_PERSONAL_TOKEN=<token>
        NOTEHUB_PROJECT_UID=app:<uid>

Usage:
    python3 notehub_upload.py <binary> [options]

Examples:
    # Version is derived from the filename ("v0.13.12")
    python3 notehub_upload.py bridge-release-v0.13.12.elf.dfu.bin

    # Release zips are supported; the .elf.dfu.bin inside is uploaded
    python3 notehub_upload.py bridge-release-v0.13.12.zip

    # Override the version and add release notes
    python3 notehub_upload.py bridge-release-v0.13.12.elf.dfu.bin \
        --version "v0.13.12" --notes "Fixes xyz"

    # Upload to a different project
    python3 notehub_upload.py bridge-release-v0.13.12.elf.dfu.bin \
        --project_uid app:<uid>

The stored filename defaults to the binary's name, and the version defaults to
the version in the bm_protocol release filename. If the filename doesn't match
the release naming, Notehub extracts the version itself.

This module can also be imported to call upload_firmware() directly.
"""

import argparse
import sys
import re
import os
import zipfile
from dataclasses import dataclass
from pathlib import Path
from dotenv import load_dotenv, find_dotenv

import notehub_py
from notehub_py.rest import ApiException

# Release images are named <name>-<build type>-<version>.elf.dfu.bin (see release.py)
RELEASE_NAME_RE = re.compile(
    r"^(?P<name>.+)-(?P<build_type>debug|release)-(?P<version>.+)$",
    re.IGNORECASE,
)
IMAGE_SUFFIX_RE = re.compile(r"(\.(elf|dfu|unified|bin|hex))+$")
NOTEHUB_IMAGE_SUFFIX = ".elf.dfu.bin"


@dataclass
class FirmwarePayload:
    filename: str
    data: bytes


def parse_args():
    parser = argparse.ArgumentParser(description="Upload firmware binary to Notehub")

    parser.add_argument("binary", type=Path, help="Path to the firmware binary")
    parser.add_argument(
        "--project_uid",
        default=os.getenv("NOTEHUB_PROJECT_UID"),
        help="Notehub project (or product) UID",
    )
    parser.add_argument(
        "--firmware_type",
        default="host",
        help="Notehub firmware type (default: host)",
    )
    parser.add_argument(
        "--filename",
        help="Name to store in Notehub (defaults to the binary's filename)",
    )
    parser.add_argument(
        "--version",
        help="Firmware version. If omitted, it is derived from the filename",
    )
    parser.add_argument("--notes", help="Notes describing this firmware version")

    return parser.parse_args()


def version_from_filename(filename):
    """Build a Notehub version like "v0.13.12 Bridge" from a release filename."""
    match = RELEASE_NAME_RE.match(IMAGE_SUFFIX_RE.sub("", filename))
    if not match:
        return None

    name = match.group("name").replace("_", " ").replace("-", " ").title()

    return f"{match.group('version')} {name}"


def release_version_from_filename(filename):
    """Pull the bare version (e.g. "v0.13.12") out of a release image filename."""
    match = RELEASE_NAME_RE.match(IMAGE_SUFFIX_RE.sub("", filename))

    return match.group("version") if match else None


def firmware_payload_from_path(binary):
    """Read a firmware payload from a binary path or bm_protocol release zip."""
    binary = Path(binary)

    if binary.suffix != ".zip":
        return FirmwarePayload(binary.name, binary.read_bytes())

    with zipfile.ZipFile(binary) as release_zip:
        images = [
            image
            for image in release_zip.namelist()
            if Path(image).name.endswith(NOTEHUB_IMAGE_SUFFIX)
        ]

        if len(images) != 1:
            raise ValueError(
                f"Expected exactly one {NOTEHUB_IMAGE_SUFFIX} image in {binary}, "
                f"found {len(images)}"
            )

        image = images[0]
        return FirmwarePayload(Path(image).name, release_zip.read(image))


def upload_firmware(
    binary,
    project_uid=None,
    firmware_type="host",
    filename=None,
    version=None,
    notes=None,
    access_token=None,
):
    """Upload a firmware binary to Notehub and return its FirmwareInfo."""
    binary = Path(binary)

    load_dotenv(find_dotenv())
    access_token = access_token or os.getenv("NOTEHUB_PERSONAL_TOKEN")
    if not access_token:
        raise ValueError("NOTEHUB_PERSONAL_TOKEN is not set")

    if not binary.is_file():
        raise FileNotFoundError(f"{binary} is not a file")

    project_uid = project_uid or os.getenv("NOTEHUB_PROJECT_UID")
    if not project_uid:
        raise ValueError("NOTEHUB_PROJECT_UID is not set")

    payload = firmware_payload_from_path(binary)
    filename = filename or payload.filename
    version = version or version_from_filename(filename)

    configuration = notehub_py.Configuration(access_token=access_token)

    with notehub_py.ApiClient(configuration) as api_client:
        api_instance = notehub_py.ProjectApi(api_client)

        print(f"Uploading {binary} to {project_uid} as {filename} ({version})")

        return api_instance.upload_firmware(
            project_uid,
            firmware_type,
            filename,
            payload.data,
            version=version,
            notes=notes,
        )


def main():
    args = parse_args()

    try:
        firmware_info = upload_firmware(
            args.binary,
            project_uid=args.project_uid,
            firmware_type=args.firmware_type,
            filename=args.filename,
            version=args.version,
            notes=args.notes,
        )
    except (ValueError, FileNotFoundError) as e:
        sys.exit(f"ERROR: {e}")
    except ApiException as e:
        sys.exit(f"ERROR: Notehub upload failed: {e}")

    print(f"Uploaded {firmware_info.filename}")


if __name__ == "__main__":
    main()
