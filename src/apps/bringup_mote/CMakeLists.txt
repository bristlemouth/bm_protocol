set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(APP_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.cpp
    PARENT_SCOPE
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
)

set(APP_LIBS
    PARENT_SCOPE
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    NO_NETWORK
    )

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
