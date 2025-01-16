set(RS232_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/rs232_expander_apps_common/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rs232_expander_apps_common/sensors/sensors.cpp
)

set(RS232_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/rs232_expander_apps_common/
    ${CMAKE_CURRENT_SOURCE_DIR}/rs232_expander_apps_common/sensors
)

set(RS232_FILES "${RS232_FILES}" PARENT_SCOPE)
set(RS232_INCLUDES "${RS232_INCLUDES}" PARENT_SCOPE)
