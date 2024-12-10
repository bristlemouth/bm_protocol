set(FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

set(FREERTOS_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/croutine.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/event_groups.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/list.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/queue.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/stream_buffer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/tasks.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/timers.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/portable/MemMang/heap_4.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
        PARENT_SCOPE)

set(FREERTOS_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/portable/GCC/ARM_CM33_NTZ/non_secure
        PARENT_SCOPE)

# Deal with library compile warnings
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Source/portable/GCC/ARM_CM4F/port.c
            DIRECTORY ${SRC_DIR}
            PROPERTIES
            COMPILE_FLAGS -Wno-int-conversion)

# Deal with library file warnings
set_source_files_properties(
            ${CMAKE_CURRENT_SOURCE_DIR}/Source/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c

            DIRECTORY ${SRC_DIR}

            PROPERTIES
            COMPILE_FLAGS -Wno-unused-parameter)
