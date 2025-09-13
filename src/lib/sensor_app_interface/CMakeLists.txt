# Sensor App Interface Library
# Shared code for sensor applications across different platforms

set(SENSOR_APP_INTERFACE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/user_code.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aanderaa_conductivity_sensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aanderaa_conductivity_sensor_util.cpp
)

set(SENSOR_APP_INTERFACE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Export for use by parent CMakeLists.txt
set(SENSOR_APP_INTERFACE_FILES "${SENSOR_APP_INTERFACE_FILES}" PARENT_SCOPE)
set(SENSOR_APP_INTERFACE_INCLUDES "${SENSOR_APP_INTERFACE_INCLUDES}" PARENT_SCOPE)
