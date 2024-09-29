from bitstring import BitStream
import struct

# Test sensor-data payloads representing different detection scenarios in hex format.
test_no_detection = '04000000'  # No detections
test_single_detection = '04000000fc3c000045002e23020041'  # Single tag
test_two_detection = '0A000000FC3C000045002E230B0041EC3C000045002E23060041'  # Two tags

payloads = [
    test_no_detection,
    test_single_detection,
    test_two_detection
]

# Description of the detection structure to unpack from the payload.
# Each tuple contains a type and a field name.
# This is a representation of the C struct the data is serialized from:
#       struct __attribute__((packed)) TagID {
#           uint32_t tag_serial_no;
#       uint16_t code_freq;
#       uint16_t code_channel;
#       uint16_t detection_count;
#       char code_char;
#       };
detect_struct_description = [
    ('uint32_t', 'tag_serial_no'),  # 4-byte unsigned integer for tag serial number.
    ('uint16_t', 'code_freq'),  # 2-byte unsigned integer for code frequency.
    ('uint16_t', 'code_channel'),  # 2-byte unsigned integer for code channel.
    ('uint16_t', 'detection_count'),  # 2-byte unsigned integer for detection count.
    ('char', 'code_char'),  # Single character code.
]


def hex_to_struct(hex_data, struct_description):
    """
    Converts hex data to a structured format using a provided struct description.

    Args:
        hex_data: The hexadecimal string or bytes to be converted.
        struct_description: A list of tuples, where each tuple defines the type
                            and name of each field in the struct.

    Returns:
        A dictionary where keys are field names from the struct description and
        values are the corresponding unpacked data.

    Raises:
        ValueError: If hex_data is neither a string nor bytes, or if the hex_data
                    does not match the expected struct size.
    """
    # Convert the hex data to bytes.
    if type(hex_data) is str:
        byte_data = bytes.fromhex(hex_data.strip())
    elif type(hex_data) is bytes:
        byte_data = hex_data
    else:
        raise ValueError(f'unsupported hex_data type: {type(hex_data)}')

    # Create the format string for struct.unpack based on the struct description.
    # Using little-endian ('<') for the format string.
    format_string = '<'
    for data_type, _ in struct_description:
        # Mapping of data types to format codes for struct.unpack.
        if data_type == 'uint8_t':
            format_string += 'B'  # Unsigned char (1 byte)
        elif data_type == 'uint16_t':
            format_string += 'H'  # Unsigned short (2 bytes)
        elif data_type == 'uint32_t':
            format_string += 'I'  # Unsigned int (4 bytes)
        elif data_type == 'uint64_t':
            format_string += 'Q'  # Unsigned long long (8 bytes)
        elif data_type == 'int8_t':
            format_string += 'b'  # Signed char (1 byte)
        elif data_type == 'int16_t':
            format_string += 'h'  # Signed short (2 bytes)
        elif data_type == 'int32_t':
            format_string += 'i'  # Signed int (4 bytes)
        elif data_type == 'int64_t':
            format_string += 'q'  # Signed long long (8 bytes)
        elif data_type == 'float':
            format_string += 'f'  # Float (4 bytes)
        elif data_type == 'double':
            format_string += 'd'  # Double (8 bytes)
        elif data_type == 'char':
            format_string += 'c'  # Single character
        else:
            raise ValueError(f"Unsupported data type: {data_type}")

    # Check the total size of the struct.
    expected_size = struct.calcsize(format_string)
    if len(byte_data) != expected_size:
        raise ValueError(f"Expected {expected_size} bytes, but got {len(byte_data)} bytes")

    # Unpack the data from the byte array based on the format string.
    values = struct.unpack(format_string, byte_data)

    # Convert the unpacked values into a dictionary with field names.
    result = {name: value for (_, name), value in zip(struct_description, values)}

    return result


if __name__ == '__main__':
    for payload in payloads:
        # Create a bitstream from the hex payload, prefixing with '0x'.
        bitstream = BitStream('0x' + payload)

        print(f"Decoding data: {bitstream}")

        # Read the number of Rx-Live status polls received (4 bytes, little-endian).
        sts_count = int(bitstream.read('uintle:32'))
        print(f"- {sts_count} Rx-Live Status Polls received.")

        # Process remaining bits in the bitstream to extract detection data.
        while bitstream.pos < bitstream.len:
            # Read the next 11 bytes representing detection data.
            detect_data = bitstream.read('bytes:11')

            # Convert detection data from bytes to a structured format.
            detection_data = hex_to_struct(detect_data, detect_struct_description)

            # Print the unpacked detection data.
            print(f"- Detection data:")
            for key in detection_data:
                print(f"\t{key}: {detection_data[key]}")

        print("---------------------------------\n")
