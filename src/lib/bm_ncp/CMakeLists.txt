#
# Bristlemouth NCP/ACP files/includes
#
set(BM_NCP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BM_NCP_FILES
    ${BM_NCP_DIR}/debug_ncp.cpp
    ${BM_NCP_DIR}/ncp_uart.cpp
    ${BM_NCP_DIR}/ncp_dfu.cpp

    PARENT_SCOPE)

set(BM_NCP_INCLUDES
    ${BM_NCP_DIR}
    PARENT_SCOPE
    )
