# Sensor Development Quick Reference

**TL;DR:** How to add a new sensor to the Bristlemouth Protocol in 5 steps.

## 🚀 Quick Start Checklist

### ✅ **Step 1: Create Shared Sensor Code**
```bash
# Create sensor driver
touch src/lib/sensor_app_interface/your_sensor.h
touch src/lib/sensor_app_interface/your_sensor.cpp
touch src/lib/sensor_app_interface/your_sensor_util.h  
touch src/lib/sensor_app_interface/your_sensor_util.cpp
```

### ✅ **Step 2: Create Platform Apps**
```bash
# Bristleback
mkdir -p src/apps/bristleback_apps/your_sensor/user_code
touch src/apps/bristleback_apps/your_sensor/user_code/user_code.h
touch src/apps/bristleback_apps/your_sensor/user_code/user_code.cpp

# RS232 Expander  
mkdir -p src/apps/rs232_expander_apps/your_sensor/user_code
touch src/apps/rs232_expander_apps/your_sensor/user_code/user_code.h
touch src/apps/rs232_expander_apps/your_sensor/user_code/user_code.cpp
```

### ✅ **Step 3: Add Bridge Support**
```bash
# Bridge sensor class
touch src/apps/bridge/sensor_drivers/yourSensor.h
touch src/apps/bridge/sensor_drivers/yourSensor.cpp

# Add to reportBuilder.cpp
# Add to reportBuilderList.cpp  
# Add to sensorController.cpp
```

### ✅ **Step 4: Create Tests**
```bash
mkdir -p test/src/apps/your_sensor
touch test/src/apps/your_sensor/CMakeLists.txt
touch test/src/apps/your_sensor/your_sensor_util_ut.cpp
```

### ✅ **Step 5: Build & Test**
```bash
# Test all platforms
cmake .. -DBSP=bridge_v1_0 -DAPP=bridge && make
cmake .. -DBSP=bm_mote_bristleback_v1_0 -DAPP=your_sensor && make  
cmake .. -DBSP=bm_mote_rs232 -DAPP=your_sensor && make

# Run tests
cd test-build && ctest -L your_sensor
```

## 📋 Code Templates

### **Sensor Driver Template** (`your_sensor.h`)
```cpp
#pragma once
#include <stdint.h>
#include "OrderedSeparatorLineParser.h"
#include "your_sensor_msg.h"

class YourSensor {
public:
    YourSensor() : _parser(",", 256, PARSER_VALUE_TYPE, NUM_FIELDS){};
    void init();
    bool getData(YourSensorMsg::Data &d);
    void flush(void);

private:
    static constexpr uint32_t BAUD_RATE = 115200;
    static constexpr ValueType PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE};
    OrderedSeparatorLineParser _parser;
    char _payload_buffer[2048];
};
```

### **Platform App Template** (`user_code.cpp`)
```cpp
#include "user_code.h"
#include "bsp.h"

SensorAppUser app;

void setup(void) {
    // Bristleback: app.setup_with_pins(&BB_VBUS_EN, &BB_PL_BUCK_EN, 500);
    // RS232:       app.setup_with_pins(&VBUS_EN, &PL_BUCK_EN, 500);
    app.setup_with_pins(&PLATFORM_VBUS_EN, &PLATFORM_PL_BUCK_EN, 500);
}

void loop(void) {
    app.loop();
}
```

### **Bridge Sensor Template** (`yourSensor.h`)
```cpp
#pragma once
#include "abstractSensor.h"

class YourSensor_t : public AbstractSensor {
public:
    YourSensor_t(uint64_t node_id, uint32_t sample_duration_ms, uint32_t max_samples);
    bool subscribe() override;
    void aggregate() override;
    
    static constexpr uint32_t N_SAMPLES_PAD = 10;
    static const your_sensor_aggregations_t YOUR_SENSOR_NAN_AGG;
};

YourSensor_t *createYourSensorSub(uint64_t node_id, uint32_t sample_duration_ms, uint32_t max_samples);
```

### **Test Template** (`your_sensor_util_ut.cpp`)
```cpp
#include <gtest/gtest.h>
#include "your_sensor_util.h"

class YourSensorUtilTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(YourSensorUtilTest, BasicFunctionality) {
    // Test your sensor utilities
    EXPECT_TRUE(true);
}
```

## 🔧 Common Patterns

### **Pin Configuration Patterns:**
```cpp
// Bristleback
app.setup_with_pins(&BB_VBUS_EN, &BB_PL_BUCK_EN, 500);

// RS232 Expander
app.setup_with_pins(&VBUS_EN, &PL_BUCK_EN, 500);

// Custom timing
app.setup_with_pins(&VBUS_EN, &PL_BUCK_EN, 1000);  // 1 second settle
```

### **Data Parser Patterns:**
```cpp
// CSV with 3 double values
static constexpr ValueType PARSER_VALUE_TYPE[] = {TYPE_DOUBLE, TYPE_DOUBLE, TYPE_DOUBLE};
OrderedSeparatorLineParser _parser(",", 256, PARSER_VALUE_TYPE, 3);

// Mixed types
static constexpr ValueType PARSER_VALUE_TYPE[] = {TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE};
```

### **CBOR Encoding Patterns:**
```cpp
// reportBuilder.cpp
case SENSOR_TYPE_YOUR_SENSOR: {
    rval = addSamplesToReport_yourSensor(context, sensor_data, sample_index);
    break;
}

static bool addSamplesToReport_yourSensor(sensor_report_encoder_context_t &context,
                                          void *sensor_data, uint32_t sample_index) {
    your_sensor_aggregations_t sample = 
        (static_cast<your_sensor_aggregations_t *>(sensor_data))[sample_index];
    
    if (sensor_report_encoder_open_sample(context, NUM_FIELDS, "bm_your_sensor_v0") != CborNoError) {
        return false;
    }
    
    ENCODE_FIELD("field1", sample.field1, TYPE_DOUBLE);
    ENCODE_FIELD("field2", sample.field2, TYPE_DOUBLE);
    
    return sensor_report_encoder_close_sample(context) == CborNoError;
}
```

## 🧪 Testing Patterns

### **Unit Test Labels:**
```cmake
set_tests_properties(your_sensor_util_tests PROPERTIES LABELS "unit;your_sensor")
```

### **Test Discovery:**
```bash
ctest -L your_sensor    # Run your sensor tests
ctest -L unit          # Run all unit tests  
ctest -L integration   # Run integration tests
```

### **Build Verification:**
```bash
# Quick build check
make your_platform-your_sensor-dbg.elf 2>&1 | grep "Linking"
```

## 📁 File Organization

```
your_sensor/
├── src/lib/sensor_app_interface/
│   ├── your_sensor.h/cpp           # Sensor driver
│   └── your_sensor_util.h/cpp      # Utilities
├── src/apps/bristleback_apps/your_sensor/user_code/
│   ├── user_code.h                 # extern SensorAppUser app;
│   └── user_code.cpp               # BB_VBUS_EN, BB_PL_BUCK_EN
├── src/apps/rs232_expander_apps/your_sensor/user_code/  
│   ├── user_code.h                 # extern SensorAppUser app;
│   └── user_code.cpp               # VBUS_EN, PL_BUCK_EN
├── src/apps/bridge/sensor_drivers/
│   ├── yourSensor.h/cpp            # Bridge sensor class
│   ├── reportBuilder.cpp           # Add CBOR encoding
│   ├── reportBuilderList.cpp       # Add pointer setup
│   └── sensorController.cpp        # Add config & discovery
└── test/src/apps/your_sensor/
    ├── CMakeLists.txt              # Test configuration
    └── your_sensor_util_ut.cpp     # Unit tests
```

## ⚡ Quick Commands

### **Build Commands:**
```bash
# Bridge
cmake .. -DBSP=bridge_v1_0 -DAPP=bridge && make bridge_v1_0-bridge-dbg.elf

# Bristleback  
cmake .. -DBSP=bm_mote_bristleback_v1_0 -DAPP=your_sensor && make bm_mote_bristleback_v1_0-your_sensor-dbg.elf

# RS232 Expander
cmake .. -DBSP=bm_mote_rs232 -DAPP=your_sensor && make bm_mote_rs232-your_sensor-dbg.elf
```

### **Test Commands:**
```bash
cd test-build
cmake -DCMAKE_BUILD_TYPE=Test ../
make your_sensor_util_tests
ctest -L your_sensor -V
```

### **Git Commands:**
```bash
git add src/ test/ docs/
git commit -m "feat: Add your_sensor support across all platforms"
```

## 🚨 Common Gotchas

### **Pin Names:**
- ❌ `VBUS_EN` on Bristleback → Use `BB_VBUS_EN`
- ❌ `BB_VBUS_EN` on RS232 → Use `VBUS_EN`
- ✅ Pass pins as parameters to `setup_with_pins()`

### **Parser Configuration:**
- ❌ Wrong field count → Parser fails silently
- ❌ Wrong ValueType → Data corruption
- ✅ Match PARSER_VALUE_TYPE array size to actual fields

### **CBOR Encoding:**
- ❌ Wrong field count → CBOR encoding fails
- ❌ Missing `close_sample()` → Malformed CBOR
- ✅ Always check return values

### **Test Configuration:**
- ❌ Wrong include paths → Compilation fails
- ❌ Missing test labels → Tests not discoverable
- ✅ Point tests to shared code location

## 📚 Reference Links

- **[Full Integration Guide](SENSOR_INTEGRATION_GUIDE.md)** - Complete step-by-step guide
- **[Architecture Overview](SHARED_SENSOR_ARCHITECTURE.md)** - Understanding the design
- **[Future Vision](FUTURE_SENSOR_VISION.md)** - Where we're heading
- **[Library README](../src/lib/sensor_app_interface/README.md)** - API documentation

## 🎯 Success Criteria

Your sensor is ready when:
- ✅ All 3 platforms build successfully
- ✅ Unit tests pass (`ctest -L your_sensor`)
- ✅ Bridge can discover and report sensor data
- ✅ No code duplication between platforms
- ✅ Follows established patterns

**Happy sensor development!** 🚀
