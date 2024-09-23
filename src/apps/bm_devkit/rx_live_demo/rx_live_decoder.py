from bitstring import BitStream
import struct

test_no_detection = '04000000'
test_single_detection = '03000000FC3C000045002E23020041'
test_two_detection = '03000000FC3C000045002E230B0041FC3C000045002E23060041'

bitstream = BitStream('0x' + test_single_detection)

print(bitstream)

struct_description = [
    ('uint16_t', 'sample_count'),
    ('float', 'mean'),
    ('float', 'stdev'),
]

detect_struct_description = [
    ('uint32_t', 'tag_serial_no'),
    ('uint16_t', 'code_freq'),
    ('uint16_t', 'code_channel'),
    ('uint16_t', 'detection_count'),
    ('char', 'code_char'),
]

def hex_to_struct(hex_data, struct_description):
    # Convert the hex data to bytes.
    if type(hex_data) is str:
        byte_data = bytes.fromhex(hex_data.strip())
    elif type(hex_data) is bytes:
        byte_data = hex_data
    else:
        raise ValueError(f'unsupported hex_data type: {type(hex_data)}')

    # Create the format string for struct.unpack based on the struct description.
    # '>' is used for big endian (the most significant byte is stored in the smallest address).
    # '<' is used for little endian (the least significant byte is stored in the smallest address).
    format_string = '<'
    for data_type, _ in struct_description:
        if data_type == 'uint8_t':
            format_string += 'B'  # B is the format code for unsigned char (1 byte)
        elif data_type == 'uint16_t':
            format_string += 'H'  # H is the format code for unsigned short (2 bytes)
        elif data_type == 'uint32_t':
            format_string += 'I'  # I is the format code for unsigned int (4 bytes)
        elif data_type == 'uint64_t':
            format_string += 'Q'  # Q is the format code for unsigned long long (8 bytes)
        elif data_type == 'int8_t':
            format_string += 'b'  # b is the format code for signed char (1 byte)
        elif data_type == 'int16_t':
            format_string += 'h'  # h is the format code for signed short (2 bytes)
        elif data_type == 'int32_t':
            format_string += 'i'  # i is the format code for signed int (4 bytes)
        elif data_type == 'int64_t':
            format_string += 'q'  # q is the format code for signed long long (8 bytes)
        elif data_type == 'float':
            format_string += 'f'  # f is the format code for float (4 bytes)
        elif data_type == 'double':
            format_string += 'd'  # d is the format code for double (8 bytes)
        elif data_type == 'char':
            format_string += 'c'  # 'c' format code for a single character
        else:
            raise ValueError(f"Unsupported data type: {data_type}")

    # Check the total size of the struct.
    expected_size = struct.calcsize(format_string)
    if len(byte_data) != expected_size:
        raise ValueError(f"Expected {expected_size} bytes, but got {len(byte_data)} bytes")

    # Unpack the data.
    values = struct.unpack(format_string, byte_data)

    # Convert the values to a dictionary based on the struct description.
    result = {name: value for (_, name), value in zip(struct_description, values)}

    return result


def parse_bm_raw_binary_message(raw_hex):
    parsed_message = {}
    fmt_hex = raw_hex if raw_hex.startswith("0x") else "0x" + raw_hex
    bitstream = BitStream('0x' + fmt_hex)
    message_type = bitstream.read('uint:8')
    if message_type != 222:
        print(f"Error! Expecting message of type 222, but received type {message_type}")
        exit(1)
    # internal header data
    unused_header_data = bitstream.read('uint:126')
    node_id_lower = int(bitstream.read('uint:32'))
    node_id_upper = int(bitstream.read('uint:32'))
    parsed_message['node_id'] = f"{((node_id_upper<<32)+node_id_lower):02X}"
    # internal config data
    unused_config_data = bitstream.read('uint:34')
    sensor_data = bytearray()
    while bitstream.bitpos <= (bitstream.len - 8):
        sensor_data.append(bitstream.read('uint:8'))

    if bitstream.bitpos != bitstream.len:
        print(f"{bitstream.len - bitstream.bitpos} bits left in bitstream unparsed")

    parsed_message['devkit_payload'] = sensor_data.hex()
    return parsed_message

parsed_message = parse_bm_raw_binary_message("de650efb19d5d50a01f9e92d8725e978c62238e79d2bb12295b27781fc2b013b08b64167ec3b3e2b01747b64425b9d163f")

print("Parsed raw message:")
for key in parsed_message:
    print(f"\t{key}: {parsed_message[key]}")

# TODO - get example of raw message with Rx-Live data.

sts_count = int(bitstream.read('uintle:32'))
print(f"{sts_count} Rx-Live Status Polls received.")
while bitstream.pos < bitstream.len:
    detect_data = bitstream.read('bytes:11')
    detection_data = hex_to_struct(detect_data, detect_struct_description)
    print(f"Detection data:")
    for key in detection_data:
        print(f"\t{key}: {detection_data[key]}")
