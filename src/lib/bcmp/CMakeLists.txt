#
# BCMP files/includes
#
set(BCMP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BCMP_FILES
    ${BCMP_DIR}/bcmp.cpp
    ${BCMP_DIR}/bcmp_cli.c
    ${BCMP_DIR}/bcmp_heartbeat.c
    ${BCMP_DIR}/bcmp_info.c
    ${BCMP_DIR}/bcmp_neighbors.c

    # BCL
    ${BCMP_DIR}/bcl/src/bm_config.c
    ${BCMP_DIR}/bcl/src/bm_l2.c
    ${BCMP_DIR}/bcl/src/bm_util.c
    ${BCMP_DIR}/bcl/src/bristlemouth.cpp

    PARENT_SCOPE)

set(BCMP_INCLUDES
    ${BCMP_DIR}
    ${BCMP_DIR}/bcl/include
    PARENT_SCOPE
    )
