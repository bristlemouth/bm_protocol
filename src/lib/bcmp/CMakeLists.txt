#
# BCMP files/includes
#
set(BCMP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BCMP_FILES
    ${BCMP_DIR}/bcmp_cli.cpp
    ${BCMP_DIR}/dfu/bm_dfu_client.cpp
    ${BCMP_DIR}/dfu/bm_dfu_core.cpp
    ${BCMP_DIR}/dfu/bm_dfu_host.cpp

    # Core bristlemouth
    ${BCMP_DIR}/bm/bm_l2.cpp
    ${BCMP_DIR}/bm/bm_printf.cpp
    ${BCMP_DIR}/bm/bristlemouth.cpp

    PARENT_SCOPE)

set(BCMP_INCLUDES
    ${BCMP_DIR}
    ${BCMP_DIR}/bm
    ${BCMP_DIR}/dfu
    PARENT_SCOPE
    )
