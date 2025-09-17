# Sensor Integration Guide

This guide explains how to add a new sensor to the Bristlemouth Protocol with support across all platforms (Bridge, Bristleback, RS232 Expander).

## Overview

The Bristlemouth Protocol supports a **shared sensor architecture** that eliminates code duplication while allowing platform-specific customization. This guide uses the Aanderaa Conductivity sensor as a reference implementation.

## Architecture

```
src/lib/sensor_app_interface/           # ← Shared sensor code
├── sensor_app_user.h/cpp              # ← SensorAppUser class (platform-agnostic)
├── your_sensor.h/cpp                  # ← Your sensor driver
└── your_sensor_util.h/cpp             # ← Your sensor utilities

src/apps/*/your_sensor/user_code/       # ← Platform-specific code
├── user_code.h                        # ← extern SensorAppUser app;
└── user_code.cpp                      # ← SensorAppUser app; setup() { app.setup_with_pins(...); }

src/apps/bridge/sensor_drivers/         # ← Bridge-specific sensor support
├── yourSensor.h/cpp                   # ← Bridge sensor class (AbstractSensor)
└── reportBuilder.cpp                  # ← CBOR encoding for your sensor

test/src/apps/your_sensor/              # ← Unit tests
├── CMakeLists.txt                     # ← Test configuration
└── your_sensor_util_ut.cpp            # ← Unit tests for sensor utilities
```

## Step-by-Step Integration

### Step 1: Create Shared Sensor Code

Create your sensor driver in `src/lib/sensor_app_interface/`:

**your_sensor.h:**
```cpp
#pragma once
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"
#include "your_sensor_msg.h"
#include "your_sensor_util.h"

class YourSensor {
  public:
    YourSensor() : _parser(",", 256, PARSER_VALUE_TYPE, NUM_FIELDS){};
    void init();
    bool getData(YourSensorMsg::Data &d);
    void flush(void);

  public:
    static constexpr char YOUR_SENSOR_RAW_LOG[] = "your_sensor_raw.log";

  private:
    static constexpr uint32_t BAUD_RATE = 115200;
    static constexpr char LINE_TERM = '\n';
    static constexpr ValueType PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, /* ... */};
    static constexpr char SENSOR_BM_LOG_ENABLE[] = "sensorBmLogEnable";

  private:
    uint32_t _sensorBmLogEnable = 0;
    OrderedSeparatorLineParser _parser;
    char _payload_buffer[2048];
};
```

**your_sensor.cpp:**
```cpp
#include "your_sensor.h"
#include "payload_uart.h"
#include "bsp.h"
// ... other includes

void YourSensor::init() {
    // Initialize UART, configure sensor, etc.
    PLUART::init(BAUD_RATE);
    // ... sensor-specific initialization
}

bool YourSensor::getData(YourSensorMsg::Data &d) {
    // Read and parse sensor data
    // Return true if valid data, false otherwise
}

void YourSensor::flush(void) {
    PLUART::flush();
}
```

### Step 2: Create Platform-Specific Applications

For each platform, create the user_code files:

**src/apps/bristleback_apps/your_sensor/user_code/user_code.h:**
```cpp
#pragma once
#include "sensor_app_user.h"

extern SensorAppUser app;
```

**src/apps/bristleback_apps/your_sensor/user_code/user_code.cpp:**
```cpp
#include "user_code.h"
#include "bsp.h"

SensorAppUser app;

void setup(void) {
    // Platform-specific pin configuration
    app.setup_with_pins(&BB_VBUS_EN, &BB_PL_BUCK_EN, 500);
}

void loop(void) {
    app.loop();
}
```

**src/apps/rs232_expander_apps/your_sensor/user_code/user_code.cpp:**
```cpp
#include "user_code.h"
#include "bsp.h"

SensorAppUser app;

void setup(void) {
    // Platform-specific pin configuration (different pins!)
    app.setup_with_pins(&VBUS_EN, &PL_BUCK_EN, 500);
}

void loop(void) {
    app.loop();
}
```

### Step 3: Add Bridge Support

Create bridge sensor class in `src/apps/bridge/sensor_drivers/`:

**yourSensor.h:**
```cpp
#pragma once
#include "abstractSensor.h"
#include "your_sensor_aggregations.h"

class YourSensor_t : public AbstractSensor {
public:
    YourSensor_t(uint64_t node_id, uint32_t sample_duration_ms, uint32_t max_samples);
    bool subscribe() override;
    void aggregate() override;

    static constexpr uint32_t N_SAMPLES_PAD = 10;
    static const your_sensor_aggregations_t YOUR_SENSOR_NAN_AGG;

private:
    static void yourSensorSubCallback(uint64_t node_id, const char *topic,
                                     uint16_t topic_len, const uint8_t *data,
                                     uint16_t data_len, uint8_t type, uint8_t version);
};

YourSensor_t *createYourSensorSub(uint64_t node_id, uint32_t sample_duration_ms,
                                  uint32_t max_samples);
```

### Step 4: Add CBOR Encoding Support

Add your sensor to `src/apps/bridge/reportBuilder.cpp`:

```cpp
case SENSOR_TYPE_YOUR_SENSOR: {
    rval = addSamplesToReport_yourSensor(context, sensor_data, sample_index);
    break;
}

static bool addSamplesToReport_yourSensor(sensor_report_encoder_context_t &context,
                                          void *sensor_data, uint32_t sample_index) {
    your_sensor_aggregations_t sample =
        (static_cast<your_sensor_aggregations_t *>(sensor_data))[sample_index];

    if (sensor_report_encoder_open_sample(context, YOUR_SENSOR_NUM_SAMPLE_MEMBERS,
                                          "bm_your_sensor_v0") != CborNoError) {
        return false;
    }

    // Add each sensor measurement
    if (sensor_report_encoder_add_sample_member(context, "measurement1",
                                                &sample.measurement1,
                                                SENSOR_REPORT_ENCODER_SAMPLE_MEMBER_TYPE_DOUBLE) != CborNoError) {
        return false;
    }

    // ... add other measurements

    return sensor_report_encoder_close_sample(context) == CborNoError;
}
```

### Step 5: Add Unit Tests

Create tests in `test/src/apps/your_sensor/`:

**CMakeLists.txt:**
```cmake
add_executable(your_sensor_util_tests your_sensor_util_ut.cpp)
target_sources(your_sensor_util_tests PRIVATE
    ${SRC_DIR}/lib/sensor_app_interface/your_sensor_util.cpp
)
target_include_directories(your_sensor_util_tests PRIVATE
    ${SRC_DIR}/lib/sensor_app_interface
)
target_link_libraries(your_sensor_util_tests ${GTEST_LIBRARIES})
add_test(NAME your_sensor_util_tests COMMAND your_sensor_util_tests)
set_tests_properties(your_sensor_util_tests PROPERTIES LABELS "unit;your_sensor")
```

### Step 6: Update Build Configuration

Add your sensor to the appropriate CMakeLists.txt files and ensure it builds for all platforms.

## Key Patterns

### 1. **Pin Parameterization**
- Use `setup_with_pins()` to handle platform differences
- Pass platform-specific GPIO pins as parameters
- Keep shared logic in `sensor_app_user.cpp`

### 2. **Error Handling**
- Return `false` from helper functions on error
- Use consistent logging patterns
- Handle CBOR encoding errors gracefully

### 3. **Testing**
- Create comprehensive unit tests for utilities
- Use test labels for discoverability (`ctest -L your_sensor`)
- Test both success and failure cases

### 4. **Documentation**
- Document sensor-specific configuration
- Explain data format and units
- Provide example usage

## Benefits of This Architecture

1. **✅ No Code Duplication** - Shared logic in one place
2. **✅ Platform Flexibility** - Easy to add new platforms
3. **✅ Maintainability** - Changes in one place affect all platforms
4. **✅ Testability** - Shared code can be unit tested
5. **✅ Consistency** - All sensors follow the same pattern

## Reference Implementation

See the Aanderaa Conductivity sensor implementation for a complete example:
- `src/lib/sensor_app_interface/aanderaa_conductivity_sensor.*`
- `src/apps/*/aanderaa_conductivity/user_code/`
- `src/apps/bridge/sensor_drivers/aanderaaConductivitySensor.*`
- `test/src/apps/aanderaa_conductivity/`
