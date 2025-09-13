# Future Sensor System Vision

This document outlines the long-term vision for a **config-driven sensor system** that eliminates code duplication across all sensor types and provides a unified, maintainable architecture.

## Current State vs Future Vision

### **Current State (Post-Refactoring)**
✅ **Achieved:** Aanderaa Conductivity sensor uses shared architecture  
✅ **Benefits:** 57% code reduction, clean class-based design  
❌ **Limitation:** Pattern must be manually implemented for each sensor type  
❌ **Duplication:** AbstractSensor concrete classes still have duplicated patterns  

### **Future Vision**
🎯 **Goal:** **Fully config-driven sensor system**  
🎯 **Benefit:** Add new sensors with **zero code changes**  
🎯 **Architecture:** Data-driven sensor definitions with automatic code generation  

## Vision Architecture

### **1. Config-Driven Sensor Definitions**

**Sensor Configuration File** (`sensors/aanderaa_conductivity.yaml`):
```yaml
sensor:
  name: "aanderaa_conductivity"
  type: "SENSOR_TYPE_AANDERAA_CONDUCTIVITY"
  version: "v1.0"
  
communication:
  protocol: "uart"
  baud_rate: 115200
  line_terminator: "\n"
  
data_format:
  parser: "csv"
  separator: ","
  fields:
    - name: "conductivity"
      type: "double"
      unit: "S/m"
      cbor_key: "conductivity"
    - name: "temperature" 
      type: "double"
      unit: "°C"
      cbor_key: "temperature"
    - name: "salinity"
      type: "double" 
      unit: "PSU"
      cbor_key: "salinity"
    - name: "water_density"
      type: "double"
      unit: "kg/m³"
      cbor_key: "water_density"
    - name: "sound_speed"
      type: "double"
      unit: "m/s" 
      cbor_key: "sound_speed"
    - name: "depth"
      type: "double"
      unit: "m"
      cbor_key: "depth"

power_management:
  vbus_settle_time_ms: 500
  power_sequence:
    - pin: "vbus_en"
      state: true
    - pin: "pl_buck_en" 
      state: true
    - delay_ms: 500
    - action: "read_data"
    - pin: "pl_buck_en"
      state: false
    - pin: "vbus_en"
      state: false

bridge_integration:
  cbor_message_name: "bm_aanderaa_conductivity_v0"
  aggregation_type: "statistical"
  sample_padding: 10
  
platforms:
  bristleback:
    pins:
      vbus_en: "BB_VBUS_EN"
      pl_buck_en: "BB_PL_BUCK_EN"
  rs232_expander:
    pins:
      vbus_en: "VBUS_EN" 
      pl_buck_en: "PL_BUCK_EN"
```

### **2. Unified Sensor Framework**

**Generic Sensor Class:**
```cpp
class ConfigDrivenSensor {
public:
    ConfigDrivenSensor(const SensorConfig& config);
    
    // Unified interface for all sensors
    void init();
    bool getData(SensorData& data);
    void flush();
    
    // Bridge integration
    bool addSamplesToReport(sensor_report_encoder_context_t& context, 
                           void* sensor_data, uint32_t sample_index);
    void setupReportBuilderPointers(report_builder_element_t* element,
                                   const void** nan_sample, void** dst);
    
private:
    SensorConfig _config;
    std::unique_ptr<DataParser> _parser;
    std::unique_ptr<PowerManager> _power_manager;
    std::unique_ptr<CborEncoder> _cbor_encoder;
};
```

**Sensor Registry:**
```cpp
class SensorRegistry {
public:
    static void registerSensor(const std::string& config_file);
    static ConfigDrivenSensor* createSensor(const std::string& sensor_name);
    static std::vector<std::string> getAvailableSensors();
    
private:
    static std::map<std::string, SensorConfig> _sensor_configs;
};
```

### **3. Automatic Code Generation**

**Build-Time Generation:**
```bash
# Generate sensor code from config files
./tools/generate_sensors.py sensors/*.yaml --output src/generated/

# Generated files:
src/generated/
├── sensor_types.h          # Enum definitions
├── sensor_factory.cpp      # Factory methods  
├── cbor_encoders.cpp       # CBOR encoding functions
└── aggregation_types.h     # Data structures
```

**Generated Bridge Integration:**
```cpp
// Auto-generated from aanderaa_conductivity.yaml
case SENSOR_TYPE_AANDERAA_CONDUCTIVITY: {
    return ConfigDrivenSensor::addSamplesToReport_AanderaaConductivity(
        context, sensor_data, sample_index);
}

bool ConfigDrivenSensor::addSamplesToReport_AanderaaConductivity(
    sensor_report_encoder_context_t& context, void* sensor_data, uint32_t sample_index) {
    
    auto sample = static_cast<aanderaa_conductivity_aggregations_t*>(sensor_data)[sample_index];
    
    if (sensor_report_encoder_open_sample(context, 6, "bm_aanderaa_conductivity_v0") != CborNoError) {
        return false;
    }
    
    // Auto-generated field encoding based on config
    ENCODE_FIELD(conductivity, sample.conductivity, TYPE_DOUBLE);
    ENCODE_FIELD(temperature, sample.temperature, TYPE_DOUBLE);
    ENCODE_FIELD(salinity, sample.salinity, TYPE_DOUBLE);
    ENCODE_FIELD(water_density, sample.water_density, TYPE_DOUBLE);
    ENCODE_FIELD(sound_speed, sample.sound_speed, TYPE_DOUBLE);
    ENCODE_FIELD(depth, sample.depth, TYPE_DOUBLE);
    
    return sensor_report_encoder_close_sample(context) == CborNoError;
}
```

### **4. Platform-Agnostic Applications**

**Universal Sensor App:**
```cpp
// src/apps/universal_sensor/user_code.cpp
#include "config_driven_sensor_app.h"

ConfigDrivenSensorApp app;

void setup(void) {
    // Sensor type determined by build configuration
    app.setup(SENSOR_CONFIG_NAME, PLATFORM_NAME);
}

void loop(void) {
    app.loop();
}
```

**Build Configuration:**
```cmake
# CMakeLists.txt
set(SENSOR_CONFIG_NAME "aanderaa_conductivity")
set(PLATFORM_NAME "bristleback")

# Automatically includes correct pins and configuration
configure_sensor_app(${SENSOR_CONFIG_NAME} ${PLATFORM_NAME})
```

## Implementation Phases

### **Phase 1: Sensor Framework Foundation** 
- [ ] Create `ConfigDrivenSensor` base class
- [ ] Implement YAML configuration parser
- [ ] Create sensor registry system
- [ ] Migrate Aanderaa Conductivity to config-driven approach

### **Phase 2: Code Generation Pipeline**
- [ ] Build sensor code generation tools
- [ ] Auto-generate CBOR encoders from config
- [ ] Auto-generate bridge integration code
- [ ] Auto-generate platform-specific pin mappings

### **Phase 3: Universal Sensor Applications**
- [ ] Create platform-agnostic sensor app template
- [ ] Implement dynamic sensor loading
- [ ] Add runtime sensor discovery
- [ ] Create sensor composition framework

### **Phase 4: Advanced Features**
- [ ] Config-driven data validation
- [ ] Dynamic sensor calibration
- [ ] Sensor health monitoring
- [ ] Performance optimization and caching

## Benefits of Config-Driven Approach

### **🚀 Development Velocity**
- **Zero Code for New Sensors**: Add sensors by creating config files only
- **Instant Platform Support**: New sensors automatically work on all platforms
- **Rapid Prototyping**: Test sensor configurations without compilation

### **🛡️ Quality & Reliability**
- **Consistent Behavior**: All sensors use the same tested framework
- **Reduced Bugs**: Less hand-written code means fewer opportunities for errors
- **Automatic Validation**: Config files can be validated at build time

### **🔧 Maintainability**
- **Single Source of Truth**: Sensor behavior defined in one config file
- **Easy Updates**: Change sensor behavior by editing config, not code
- **Version Control**: Sensor configurations tracked alongside code

### **📈 Scalability**
- **Unlimited Sensors**: Framework scales to hundreds of sensor types
- **Team Productivity**: Multiple developers can add sensors in parallel
- **Future-Proof**: Easy to add new platforms and features

## Migration Strategy

### **Incremental Approach:**
1. **Start with Aanderaa Conductivity**: Convert existing implementation to config-driven
2. **Add One More Sensor**: Validate framework with second sensor type
3. **Migrate Existing Sensors**: Convert remaining sensors one by one
4. **Deprecate Old Pattern**: Remove manual sensor implementations

### **Backward Compatibility:**
- Keep existing sensor implementations during transition
- Gradual migration without breaking existing functionality
- Side-by-side operation of old and new systems

### **Risk Mitigation:**
- Extensive testing at each phase
- Rollback capability to previous implementations
- Incremental deployment across platforms

## Success Metrics

### **Quantitative Goals:**
- **90% Code Reduction**: From current sensor implementations
- **10x Faster Sensor Addition**: New sensors in hours, not days
- **Zero Duplication**: Eliminate all sensor-related code duplication
- **100% Platform Coverage**: All sensors work on all platforms automatically

### **Qualitative Goals:**
- **Developer Experience**: Simple, intuitive sensor addition process
- **System Reliability**: Consistent, well-tested sensor behavior
- **Maintainability**: Easy to understand and modify sensor configurations
- **Extensibility**: Framework supports future sensor types and platforms

## Conclusion

The config-driven sensor system represents the **ultimate evolution** of the current shared sensor architecture. By moving from code-based to data-based sensor definitions, we can achieve:

- **🎯 Zero-code sensor addition**
- **🔄 Automatic platform support** 
- **📊 Dramatic code reduction**
- **🛡️ Improved reliability**
- **🚀 Enhanced developer productivity**

This vision builds upon the solid foundation established by the current Aanderaa Conductivity refactoring, taking the next logical step toward a fully automated, maintainable sensor ecosystem.

**The future is config-driven, and the path is clear.** 🌟
