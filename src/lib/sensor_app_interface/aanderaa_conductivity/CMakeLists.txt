# Aanderaa Conductivity Sensor App Interface Library
# Shared code for Bristleback and RS232 Expander conductivity sensor applications

set(LIB_NAME aanderaa_conductivity_app_interface)

set(SOURCES
    AanderaaConductivityApp.cpp
    AanderaaConductivitySensor.cpp
    AanderaaConductivitySensorUtil.cpp
)

set(HEADERS
    AanderaaConductivityApp.h
    AanderaaConductivitySensor.h
    AanderaaConductivitySensorUtil.h
    ../SensorApp.h
)

add_library(${LIB_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${LIB_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # For SensorApp.h
    PRIVATE
        # Include paths from main lib system
        ${CMAKE_SOURCE_DIR}/src/lib/common
        ${CMAKE_SOURCE_DIR}/src/lib/bristlefin
        ${CMAKE_SOURCE_DIR}/src/lib/bridge
        ${CMAKE_SOURCE_DIR}/src/lib/drivers/include
        ${CMAKE_SOURCE_DIR}/src/lib/middleware
        ${CMAKE_SOURCE_DIR}/src/bmcore/bm_common_messages
        ${CMAKE_SOURCE_DIR}/src/third_party/FreeRTOS-Plus-CLI
        # BSP include path - this will be set by the main build system
        ${CMAKE_SOURCE_DIR}/src/bsp/${BSP}
)

target_link_libraries(${LIB_NAME}
    PUBLIC
        # These are needed by the interface headers
        bmcore
        bmmessages
)

# Export the library for use by sensor applications
set_target_properties(${LIB_NAME} PROPERTIES
    EXPORT_NAME AanderaaConductivityAppInterface
)
