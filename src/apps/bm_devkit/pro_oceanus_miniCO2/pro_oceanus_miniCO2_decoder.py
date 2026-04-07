"""
Pro-Oceanus miniCO2 Sensor Data Decoding Script

Decodes hexadecimal payloads from Pro-Oceanus miniCO2 sensor messages received via the sensor-data Sofar API.
Parses binary-encoded aggregated CO2 statistics into human-readable fields including sample count,
min, max, mean, and standard deviation of corrected CO2 readings.

Usage:
- Define a hex payload (e.g., from sensor-data API).
- Each sample is printed with corresponding field names.

Dependencies:
- bitstring
- struct

"""


from bitstring import BitStream
import struct

# Test sensor-data payloads in hex format.
# Replace these with your actual payloads
test_sample = '96000000000000e86ec01f85eb51b8db9e40107d64600a7498406843800ba97f8a40'

payloads = [
    test_sample,
]

# Description of the co2Data_t structure to unpack from the payload.
# Each tuple contains a type and a field name.
# This is a representation of the C struct the data is serialized from:
#       typedef struct {
#           uint16_t sample_count;
#           double min;
#           double max;
#           double mean;
#           double stdev;
#       } __attribute__((__packed__)) co2Data_t;
detect_struct_description = [
    ('uint16_t', 'sample_count'),   # Number of samples in aggregation period
    ('double', 'min'),              # Minimum corrected CO2 (ppmv)
    ('double', 'max'),              # Maximum corrected CO2 (ppmv)
    ('double', 'mean'),             # Mean corrected CO2 (ppmv)
    ('double', 'stdev'),            # Standard deviation of corrected CO2 (ppmv)
]

# Calculate the sample size: uint16_t (2 bytes) + 4 doubles (8 bytes each) = 34 bytes
SAMPLE_SIZE = 2 + (4 * 8)  # 34 bytes


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
        # Convert hex string directly to bytes
        byte_data = bytes.fromhex(payload)

        print(f"Decoding payload: {payload}")
        print(f"Payload size: {len(byte_data)} bytes")

        # Convert sample data from bytes to a structured format.
        sample = hex_to_struct(byte_data, detect_struct_description)

        # Print the unpacked sample data.
        print(f"- miniCO2 Aggregated Data:")
        print(f"\tsample_count: {sample['sample_count']}")
        print(f"\tmin: {sample['min']:.2f} ppmv")
        print(f"\tmax: {sample['max']:.2f} ppmv")
        print(f"\tmean: {sample['mean']:.2f} ppmv")
        print(f"\tstdev: {sample['stdev']:.2f} ppmv")

        print("---------------------------------\n")
