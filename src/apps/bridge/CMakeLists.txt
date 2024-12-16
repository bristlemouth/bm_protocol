set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sm_config_crc_list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bridgePowerController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor_drivers/aanderaaSensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor_drivers/rbrCodaSensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor_drivers/softSensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor_drivers/seapointTurbiditySensor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors/sensors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sensorController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bridgeLog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cbor_sensor_report_encoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/reportBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/network_config_logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/topology_sampler.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sensor_drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/sensors
)

set(APP_LIBS
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    # Add network stress test functions
    STRESS_TEST_ENABLE
    )

if (BRIDGE_AUTO_ENABLE STREQUAL 1)
list(APPEND APP_DEFINES "BRIDGE_AUTO_ENABLE")
message(STATUS "AUTO ENABLE BRIDGE 24V!")
endif()

if (RAW_PRESSURE_ENABLE STREQUAL 1)
list(APPEND APP_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/rbrPressureProcessor.cpp)
list(APPEND APP_FILES  ${SRC_DIR}/lib/common/differenceSignal.cpp)
list(APPEND APP_DEFINES "RAW_PRESSURE_ENABLE")
endif()

# Send APP_* defines to parent scope (otherwise we can't append to the lists above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
