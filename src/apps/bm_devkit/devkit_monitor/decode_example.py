# Example Python decoder of data sent by this App.

# Manually specify the schema and size for your encoding:
struct_description = [
    ('uint16_t', 'sample_count'),
    ('double', 'min'),
    ('double', 'max'),
    ('double', 'mean'),
    ('double', 'stdev'),
]

STRUCT_SIZE_BYTES = 34

## --------------------------------------------

import struct

def hex_to_struct(hex_data, struct_description):
    # Convert the hex data to bytes
    byte_data = bytes.fromhex(hex_data.strip())

    # Create the format string for struct.unpack based on the struct description
    # '>' is used for big endian(most significant byte is stored in the smallest address)
    # '<' is used for little endian(least significant byte is stored in the smallest address)
    format_string = '<'
    for data_type, _ in struct_description:
        if data_type == 'uint16_t':
            format_string += 'H'  # H is the format code for ushort in struct
        elif data_type == 'double':
            format_string += 'd'  # d is the format code for double in struct
        elif data_type == 'float':
            format_string += 'f'  # f is the format code for double in struct
        else:
            raise ValueError(f"Unsupported data type: {data_type}")

    # Check the total size of the struct
    expected_size = struct.calcsize(format_string)
    if len(byte_data) != expected_size:
        raise ValueError(f"Expected {expected_size} bytes, but got {len(byte_data)} bytes")

    # Unpack the data
    values = struct.unpack(format_string, byte_data)

    # Convert the values to a dictionary based on the struct description
    result = {name: value for (_, name), value in zip(struct_description, values)}

    return result

hex_data = "2f00000000401d8e39400000004003424040932b88a92ab33c40fb7fab28597802402f000000000030e54440000000000cbe5d40c4e40a627a614d40223c71f0b1ee3240300000000000ccee3740000000e034ef374055555511fcee3740cfa13562a7204a3f30000000000052b87e3f000000e076be7f3f0000004eb4c87e3ffff42a713cba0f3f2f000000008014d48e4000000080eb5b8f401bf59ded67d98e402ddd6d0b6b460340"
sensor_names = ["temp", "hum", "voltage", "current", "pressure"]

for i in range(len(sensor_names)):
    hex_data = hex_data.replace(" ", "").replace("\n", "")
    struct_data = hex_data[i*STRUCT_SIZE_BYTES*2:(i+1)*STRUCT_SIZE_BYTES*2]
    print(sensor_names[i])
    print(hex_to_struct(struct_data, struct_description))

# script output:
# temp
# {'sample_count': 47, 'min': 25.555133819580078, 'max': 32.515724182128906, 'mean': 28.69986972403019, 'stdev': 2.308763807042849}
# hum
# {'sample_count': 47, 'min': 41.79052734375, 'max': 118.969482421875, 'mean': 58.76154733211436, 'stdev': 18.932402637143362}
# voltage
# {'sample_count': 48, 'min': 23.93280029296875, 'max': 23.93440055847168, 'mean': 23.933533747990925, 'stdev': 0.0007973496725926457}
# current
# {'sample_count': 48, 'min': 0.007500000298023224, 'max': 0.007750000339001417, 'mean': 0.007515625300584361, 'stdev': 6.0515374703732146e-05}
# pressure
# {'sample_count': 47, 'min': 986.510009765625, 'max': 1003.489990234375, 'mean': 987.1757461872506, 'stdev': 2.40938385895756}

# SD log line:
# 2023-07-16T02:31:06.277Z | rtc_start: 2023-07-16T02:27:05.359, tick_start: 361, tick_end: 240369,
# temp_n: 47, temp_min: 25.5551, temp_max: 32.5157, temp_mean: 28.6999, temp_std: 2.3088,
# hum_n: 47, hum_min: 41.7905, hum_max: 118.9695, hum_mean: 58.7615, hum_std: 18.9324,
# voltage_n: 48, voltage_min: 23.9328, voltage_max: 23.9344, voltage_mean: 23.9335, voltage_std: 0.0008,
# current_n: 48, current_min: 0.0075, current_max: 0.0078, current_mean: 0.0075, current_std: 0.0001,
# pressure_n: 47, pressure_min: 986.5100, pressure_max: 1003.4900, pressure_me
