set(BMDK_COMMON ${CMAKE_CURRENT_LIST_DIR}/../bm_devkit/bmdk_common)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)

# Shared Dev Kit boilerplate, referenced in place rather than copied. These are the
# same files src/CMakeLists.txt pulls in via DEV_KIT_FILES when CMAKE_APP_TYPE=BMDK.
set(APP_FILES
    ${BMDK_COMMON}/app_main.cpp
    ${BMDK_COMMON}/sensors/sensors.cpp
    ${CMAKE_CURRENT_LIST_DIR}/user_code/user_code.cpp
    # TEMPORARY DEBUG - PHY register dumper; revert together with the rest of
    # the link-up-stall instrumentation.
    ${CMAKE_CURRENT_LIST_DIR}/user_code/adin_phy_report.cpp
)

set(APP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/user_code
)

# bm_core's middleware target finds FreeRTOSConfig.h / memfault_platform_config.h
# through APP_DIR_COMMON (see BM_CORE_FREERTOS_INCLUDES in src/CMakeLists.txt).
set(APP_DIR_COMMON
    ${BMDK_COMMON}
    ${BMDK_COMMON}/sensors
)

set(APP_LIBS
)

set(APP_DEFINES
    APP_NAME="${APP_NAME}"
    # STRESS_TEST_ENABLE deliberately omitted. It spawns a "stress" task that
    # exercises the network device - pure noise for a harness whose whole job
    # is to measure how long bring-up takes on an otherwise idle system.
    # adin2111_init() calls adin2111_network_metrics_init() on every invocation
    # (bm_adin2111.c), which calls metrics_service_add_component() once per port.
    # That function does not deduplicate by key - it ll_create_item()s a fresh
    # heap-allocated entry every time (metrics_service.c:113) - so this harness,
    # which re-inits once per cycle, leaks 4 allocations per cycle forever and
    # grows the metrics component list without bound. Production only inits
    # once, so it never notices. The harness does not need metrics; compiling
    # them out removes the leak at the source. bm_config.h guards the default
    # with #ifndef, so this override wins.
    bm_metrics_enabled=0
)

# Send APP_DEFINES to parent scope (otherwise we can't append to the
# list above)
set(APP_DEFINES "${APP_DEFINES}" PARENT_SCOPE)
set(APP_LIBS "${APP_LIBS}" PARENT_SCOPE)
set(APP_FILES "${APP_FILES}" PARENT_SCOPE)
set(APP_INCLUDES "${APP_INCLUDES}" PARENT_SCOPE)
set(APP_DIR_COMMON "${APP_DIR_COMMON}" PARENT_SCOPE)
