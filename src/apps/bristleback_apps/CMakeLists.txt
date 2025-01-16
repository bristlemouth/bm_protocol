set(BRISTLEBACK_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/bristleback_apps_common/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bristleback_apps_common/sensors/sensors.cpp
)

set(BRISTLEBACK_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/bristleback_apps_common/
    ${CMAKE_CURRENT_SOURCE_DIR}/bristleback_apps_common/sensors
)

set(BRISTLEBACK_FILES "${BRISTLEBACK_FILES}" PARENT_SCOPE)
set(BRISTLEBACK_INCLUDES "${BRISTLEBACK_INCLUDES}" PARENT_SCOPE)
