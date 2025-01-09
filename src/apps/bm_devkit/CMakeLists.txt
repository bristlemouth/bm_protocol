set(DEV_KIT_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/bmdk_common/app_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bmdk_common/sensors/sensors.cpp
)

set(DEV_KIT_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/bmdk_common
    ${CMAKE_CURRENT_SOURCE_DIR}/bmdk_common/sensors
)

set(DEV_KIT_FILES "${DEV_KIT_FILES}" PARENT_SCOPE)
set(DEV_KIT_INCLUDES "${DEV_KIT_INCLUDES}" PARENT_SCOPE)
