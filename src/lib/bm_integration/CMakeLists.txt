#
# BCMP files/includes
#
set(BM_INTEGRATION_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(BM_INTEGRATION_FILES
    ${BM_INTEGRATION_DIR}/bcmp_cli.cpp

    # Core bristlemouth
    ${BM_INTEGRATION_DIR}/bm_printf.cpp
    ${BM_INTEGRATION_DIR}/bristlemouth_client.cpp

    PARENT_SCOPE)

set(BM_INTEGRATION_INCLUDES
    ${BM_INTEGRATION_DIR}
    PARENT_SCOPE
    )
