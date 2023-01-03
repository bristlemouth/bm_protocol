# Inspired by https://www.mattkeeter.com/blog/2018-01-06-versioning/

#
# Include version.h in your code to use the auto-generated variables
# Make sure to update version.h if you ever add new ones!
#

set(GIT_CMD git)
set(GIT_ARGS_SHA describe --match ForceNone --abbrev=8 --always)
execute_process(COMMAND ${GIT_CMD} ${GIT_ARGS_SHA}
                OUTPUT_VARIABLE GIT_SHA
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Figure out if there are uncommitted/unstaged changes to git repo
# See https://stackoverflow.com/a/2659808 for more info
execute_process(COMMAND ${GIT_CMD} diff-index --quiet HEAD -- RESULT_VARIABLE IS_DIRTY)
if(IS_DIRTY)
set(DIRTY_STR "1")
else()
set(DIRTY_STR "0")
endif()

# if VERSION_STR wasn't passed as an argument, get it from git
if(NOT VERSION_STR)
    set(GIT_ARGS_TAG describe --always --dirty --abbrev=8)

    execute_process(COMMAND ${GIT_CMD} ${GIT_ARGS_TAG}
                    OUTPUT_VARIABLE VERSION_STR
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# Convert GIT_SHA from hex to decimal
math(EXPR GIT_SHA_DEC "0x${GIT_SHA}" OUTPUT_FORMAT DECIMAL)

set(NOTE_FW_STR "\n#ifdef HAS_NOTECARD\nconst char *NOTE_FW_STR = \"firmware::info:{")
set(NOTE_FW_STR "${NOTE_FW_STR}\\\"product\\\":\\\"Sunflower\\\",")
set(NOTE_FW_STR "${NOTE_FW_STR}\\\"firmware\\\":\\\"${VERSION_STR}\\\",")

# Default to eng build unless RELEASE=1 is passed in
if(RELEASE STREQUAL "1")
set(ENG_STR "0")
else ()
set(ENG_STR "1")
endif()

if (VERSION_STR MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)$")
    # Extract version info from git tag
    # Expects format vx.y.z with optional extra stuff in the end
    set(MAJ_STR "${CMAKE_MATCH_1}")
    set(MIN_STR "${CMAKE_MATCH_2}")
    set(REV_STR "${CMAKE_MATCH_3}")

    set(NOTE_FW_STR "${NOTE_FW_STR}\\\"ver_major\\\":${CMAKE_MATCH_1},")
    set(NOTE_FW_STR "${NOTE_FW_STR}\\\"ver_minor\\\":${CMAKE_MATCH_2},")
    set(NOTE_FW_STR "${NOTE_FW_STR}\\\"ver_patch\\\":${CMAKE_MATCH_3},")

    set(MCUBOOT_VERSION_STR "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}+${GIT_SHA_DEC}")

    # Don't allow invalid version strings when doing releases
    # ENG- releases can have whatever
    if(CMAKE_MATCH_4 AND RELEASE STREQUAL "1")
        message(ERROR "Release versions MUST follow format: vx.x.x")
    endif()

else()
    message(WARNING "invalid version string!")

    set(NOTE_FW_STR "${NOTE_FW_STR}\\\"ver_major\\\":0,")
    set(NOTE_FW_STR "${NOTE_FW_STR}\\\"ver_minor\\\":0,")
    set(NOTE_FW_STR "${NOTE_FW_STR}\\\"ver_patch\\\":0,")

    set(MAJ_STR "0xFF")
    set(MIN_STR "0xFF")
    set(REV_STR "0xFF")
    set(MCUBOOT_VERSION_STR "0.0.0+${GIT_SHA_DEC}")
endif()

# Safety checks
if(DIRTY_STR STREQUAL "1" AND RELEASE STREQUAL 1)
message( SEND_ERROR "Cannot have a release build with a dirty git repo!" )
endif()

# Last item doesn't have a comma!!!
set(NOTE_FW_STR "${NOTE_FW_STR}\\\"ver_build\\\":${GIT_SHA_DEC}")
set(NOTE_FW_STR "${NOTE_FW_STR}}\";\n#endif\n")

#
# These arguments are passed as command line defines -DXYZ by caller
#
if(APP STREQUAL "bootloader")
    set(IS_BOOTLOADER_STR "1")
else()
    set(IS_BOOTLOADER_STR "0")
endif()

if(SIGN_IMAGES STREQUAL 1)
    set(SIGN_IMAGES_STR "1")
else()
    set(SIGN_IMAGES_STR "0")
endif()

if(ENCRYPT_IMAGES STREQUAL 1)
    set(ENCRYPT_IMAGES_STR "1")
else()
    set(ENCRYPT_IMAGES_STR "0")
endif()

string(LENGTH ${VERSION_STR} VERSION_STR_LEN)

string(CONCAT VERSION
    "#include <stdbool.h>\n"
    "#include <stdint.h>\n"
    "#include \"version.h\"\n"
    "\n"
    "__attribute__ ((section(\".note.sofar.version\")))\n"
    "const versionNote_t versionNote = {\n"
    "  .note = {\n"
    "    .namesz=sizeof(((versionNote_t *)0)->name),\n"
    "    .descsz=sizeof(versionInfo_t),\n"
    "    .type=0x10, // Arbitrarily large value so binutis won't get confused\n"
    "  },\n"
    "  .name=\"VERSION\",\n"
    "  .info = {\n"
    "    .magic = VERSION_MAGIC,\n"
    "    .gitSHA = 0x${GIT_SHA},\n"
    "    .maj = ${MAJ_STR},\n"
    "    .min = ${MIN_STR},\n"
    "    .rev = ${REV_STR},\n"
    "    .hwVersion = ${HW_VERSION},\n"
    "    .flags = (\n"
    "      (${ENG_STR} << VER_ENG_FLAG_OFFSET) |\n"
    "      (${DIRTY_STR} << VER_DIRTY_FLAG_OFFSET) |\n"
    "      (${IS_BOOTLOADER_STR} << VER_IS_BOOTLOADER_OFFSET) |\n"
    "      (${SIGN_IMAGES_STR} << VER_SIGNATURE_SUPPORT_OFFSET) |\n"
    "      (${ENCRYPT_IMAGES_STR} << VER_ENCRYPTION_SUPPORT_OFFSET)\n"
    "     ),\n"
    "    .versionStrLen = ${VERSION_STR_LEN},\n"
    "    .versionStr = \"${VERSION_STR}\",\n"
    "  },\n"
    "};\n"
    "${NOTE_FW_STR}\n"
    )


if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/version.c)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.c VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/version.c "${VERSION}")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/version_string)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version_string MCUBOOT_VERSION_STR_)
else()
    set(MCUBOOT_VERSION_STR_ "")
endif()

if (NOT "${MCUBOOT_VERSION_STR}" STREQUAL "${MCUBOOT_VERSION_STR_}")
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/version_string "${MCUBOOT_VERSION_STR}")
endif()

message(STATUS "git version: ${VERSION_STR}")
