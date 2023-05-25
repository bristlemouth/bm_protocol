# Create an INTERFACE library for our C module.
add_library(device INTERFACE)

# Add our source files to the lib
target_sources(device INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/usermod_device.c
)

# Add the current directory as an include directory.
target_include_directories(device INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE device)
