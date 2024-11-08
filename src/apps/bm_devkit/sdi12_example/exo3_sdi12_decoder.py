from bitstring import BitStream
import struct

# Test sensor-data payloads representing different detection scenarios in hex format.
payload = "de672c10f5da00ac0218d12dc645e96b7ccaf3e2ef0ef2e48813d2dbfc83c0b741d7a3b84048e1e24033339dc1e17acf4266660e416666564148e19a3fe3a59b3e85eb4141f2d2b7415c8fba4048e1e240295c9dc1e17acf4266660e411f8553419a99993fe3a59b3e85eb414160e5b741cdccb44048e1e240d7a39cc1c375cf4266660e4185eb594148e19a3ff6289c3e85eb4141"

# Description of the detection structure to unpack from the payload.
# Each tuple contains a type and a field name.
# This is a representation of the C struct the data is serialized from:
#       struct __attribute__((packed)) EXO3sample {
#               float temp_sensor;    // Celcius
#               float sp_cond;        // uS/cm
#               float pH;
#               float pH_mV;          // mV
#               float dis_oxy;        // % Sat
#               float dis_oxy_mg;     // mg/L
#               float turbidity;      // NTU
#               float wiper_pos;      // volt
#               float depth;          // meters
#               float power;          // volt
#             };
detect_struct_description = [
    ('float', 'temp_sensor'),       # Celcius
    ('float', 'sp_cond'),           # uS/cm
    ('float', 'pH'),                # range: 1-14
    ('float', 'pH_mV'),             # mV
    ('float', 'dis_oxy'),           # % Sat
    ('float', 'dis_oxy_mg'),        # mg/L
    ('float', 'turbidity'),         # NTU
    ('float', 'wiper_pos'),         # Volt
    ('float', 'depth'),             # meters
    ('float', 'power'),             # Volt
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
    # Your hexadecimal data as bytes
    hex_data = bytes.fromhex(payload)

    # Load into a BitStream
    bitstream = BitStream(hex_data)

    # To read only raw data, skip the first 29 header bytes by reading and discarding them
    _ = bitstream.read('bytes:29')

    # Process remaining bits in the bitstream to extract detection data.
    while bitstream.pos < bitstream.len:
        # Read the next 11 bytes representing detection data.
        detect_data = bitstream.read('bytes:40')

        # Convert detection data from bytes to a structured format.
        detection_data = hex_to_struct(detect_data, detect_struct_description)

        # Print the unpacked detection data.
        print(f"- Detection data:")
        for key in detection_data:
            print(f"\t{key}: {detection_data[key]}")

    print("---------------------------------\n")
