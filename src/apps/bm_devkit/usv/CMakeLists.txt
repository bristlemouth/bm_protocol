set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(APP_FILES
    ${CMAKE_CURRENT_LIST_DIR}/user_code/user_code.cpp
    ${CMAKE_CURRENT_LIST_DIR}/user_code/solar_scout_comms.cpp
    ${CMAKE_CURRENT_LIST_DIR}/user_code/debug_solar_scout.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/user_code
)

add_subdirectory(sss)

set(APP_LIBS
    sss
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
)



# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
