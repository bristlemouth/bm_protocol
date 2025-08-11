set(CTENOPHORA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ctenophora_common/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ctenophora_common/sensors/sensors.cpp
)

set(CTENOPHORA_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/ctenophora_common/
    ${CMAKE_CURRENT_SOURCE_DIR}/ctenophora_common/sensors
)

set(CTENOPHORA_FILES "${CTENOPHORA_FILES}" PARENT_SCOPE)
set(CTENOPHORA_INCLUDES "${CTENOPHORA_INCLUDES}" PARENT_SCOPE)
