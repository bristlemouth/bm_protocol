from bitstring import BitStream
import struct

# Parse raw messages in hex format
raw_message = "de68753833b61bd40279192dc62de96aff0746ad5e0665b2b19ecc6db43d0ab9417b142ebe00000000295c8fbe85ebba42000000418fc2a540ec51983f81952541295c2f404c37b9410ad7233d00000000e17a94bef628bb42f6280041cdcca4403d0a973f06812541000030407f6ab941cdcc4c3d00000000f628dcbe5238bb4200000041e17aa4403d0a973fd578254100003040"
# Parse sensor data in hex format 
sensor_data = "3d0ab9417b142ebe00000000295c8fbe85ebba42000000418fc2a540ec51983f81952541295c2f404c37b9410ad7233d00000000e17a94bef628bb42f6280041cdcca4403d0a973f06812541000030407f6ab941cdcc4c3d00000000f628dcbe5238bb4200000041e17aa4403d0a973fd578254100003040"

# Description of the detection structure to unpack from the EXO3 sonde.
# Each tuple contains a type and a field name.
# This is a representation of the C struct the data is serialized from:
#       struct __attribute__((packed)) EXO3sample {
#               float temp_sensor;    // Celcius
#               float sp_cond;        // μS/cm
#               float phyocyanin;     // μg/L
#               float chlorophyll;    // μg/L
#               float dis_oxy;        // % Sat
#               float dis_oxy_mg;     // mg/L
#               float turbidity;      // NTU
#               float wiper_pos;      // volt
#               float depth;          // meters
#               float power;          // volt
#             };
detect_struct_description = [
    ('float', 'temp_sensor (ºC)'),      # Celcius
    ('float', 'sp_cond (μS/cm)'),       # μS/cm
    ('float', 'phycocyanin (μg/L)'),    # μg/L
    ('float', 'chlorophyll (μg/L)'),    # μg/L
    ('float', 'dis_oxy (% Sat)'),       # % Sat
    ('float', 'dis_oxy_mg (mg/L)'),     # mg/L
    ('float', 'turbidity (NTU)'),       # NTU
    ('float', 'wiper_pos (V)'),         # Volt
    ('float', 'depth (m)'),             # meters
    ('float', 'batt_level (V)'),        # Volt
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
    raw_hex_data = bytes.fromhex(raw_message)
    sensor_hex_data = bytes.fromhex(sensor_data)

    # Load into a BitStream
    raw_bitstream = BitStream(raw_hex_data)
    sensor_bitstream = BitStream(sensor_hex_data)

    # To read only raw data, skip the first 29 header bytes by reading and discarding them. This is not needed for sensor data
    _ = raw_bitstream.read('bytes:29')

    print("~~~~~~~~~ FROM RAW MESSAGES END POINT ~~~~~~~~~")

    # Process remaining bits in the bitstream to extract detection data.
    while raw_bitstream.pos < raw_bitstream.len:
        # Read the next 11 bytes representing detection data.
        raw_detect_data = raw_bitstream.read('bytes:40')

        # Convert detection data from bytes to a structured format.
        raw_detection_data = hex_to_struct(raw_detect_data, detect_struct_description)

        # Print the unpacked detection data.
        print(f"- Detection data:")
        for key in raw_detection_data:
            print(f"\t{key}: {float(raw_detection_data[key]):.3f}")

        print("---------------------------------\n")

    print("~~~~~~~~~ FROM SENSOR DATA END POINT ~~~~~~~~~")

    # Process remaining bits in the bitstream to extract detection data.
    while sensor_bitstream.pos < sensor_bitstream.len:
        # Read the next 11 bytes representing detection data.
        sensor_detect_data = sensor_bitstream.read('bytes:40')

        # Convert detection data from bytes to a structured format.
        sensor_detection_data = hex_to_struct(sensor_detect_data, detect_struct_description)

        # Print the unpacked detection data.
        print(f"- Detection data:")
        for key in sensor_detection_data:
            print(f"\t{key}: {float(sensor_detection_data[key]):.3f}")

        print("---------------------------------\n")