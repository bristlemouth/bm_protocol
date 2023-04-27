#
# BCMP files/includes
#
set(BCMP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BCMP_FILES
    ${BCMP_DIR}/bcmp.cpp
    ${BCMP_DIR}/bcmp_cli.cpp
    ${BCMP_DIR}/bcmp_heartbeat.cpp
    ${BCMP_DIR}/bcmp_info.cpp
    ${BCMP_DIR}/bcmp_neighbors.cpp
    ${BCMP_DIR}/bcmp_ping.cpp
    ${BCMP_DIR}/dfu/src/bm_dfu_client.cpp
    ${BCMP_DIR}/dfu/src/bm_dfu_core.cpp
    ${BCMP_DIR}/dfu/src/bm_dfu_host.cpp

    # BCL
    ${BCMP_DIR}/bcl/src/bm_config.c
    ${BCMP_DIR}/bcl/src/bm_l2.cpp
    ${BCMP_DIR}/bcl/src/bm_util.c
    ${BCMP_DIR}/bcl/src/bristlemouth.cpp

    PARENT_SCOPE)

set(BCMP_INCLUDES
    ${BCMP_DIR}/dfu/include
    ${BCMP_DIR}
    ${BCMP_DIR}/bcl/include
    PARENT_SCOPE
    )
