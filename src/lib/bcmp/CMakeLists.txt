#
# BCMP files/includes
#
set(BCMP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BCMP_FILES
    ${BCMP_DIR}/bcmp.cpp
    ${BCMP_DIR}/bcmp_cli.cpp
    ${BCMP_DIR}/bcmp_config.cpp
    ${BCMP_DIR}/bcmp_time.cpp
    ${BCMP_DIR}/bcmp_heartbeat.cpp
    ${BCMP_DIR}/bcmp_info.cpp
    ${BCMP_DIR}/bcmp_neighbors.cpp
    ${BCMP_DIR}/bcmp_ping.cpp
    ${BCMP_DIR}/dfu/bm_dfu_client.cpp
    ${BCMP_DIR}/dfu/bm_dfu_core.cpp
    ${BCMP_DIR}/dfu/bm_dfu_host.cpp

    # Core bristlemouth
    ${BCMP_DIR}/bm/bm_config.c
    ${BCMP_DIR}/bm/bm_l2.cpp
    ${BCMP_DIR}/bm/bm_util.c
    ${BCMP_DIR}/bm/bm_printf.c
    ${BCMP_DIR}/bm/bristlemouth.cpp

    PARENT_SCOPE)

set(BCMP_INCLUDES
    ${BCMP_DIR}
    ${BCMP_DIR}/bm
    ${BCMP_DIR}/dfu
    PARENT_SCOPE
    )
