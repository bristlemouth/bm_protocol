#
# BCMP files/includes
#
set(BCMP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BCMP_FILES
    ${BCMP_DIR}/bcmp.c
    ${BCMP_DIR}/bcmp_cli.c
    ${BCMP_DIR}/bcmp_heartbeat.c
    ${BCMP_DIR}/bcmp_neighbors.c

    PARENT_SCOPE)

set(BCMP_INCLUDES
    ${BCMP_DIR}
    PARENT_SCOPE
    )
