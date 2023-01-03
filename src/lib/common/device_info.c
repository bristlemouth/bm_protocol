#include <stdio.h>
#include <string.h>
#include "device_info.h"
// #include "stm32l4xx_hal.h"
#include "stm32u5xx_hal.h"

extern uint8_t __start_gnu_build_id_start[];
const ElfNoteSection_t *g_note_build_id = (ElfNoteSection_t *)__start_gnu_build_id_start;
const uint32_t *UID = (const uint32_t *)(UID_BASE); // 96-bit unique hardware id

static char fwVersionStr[128];
static char uidStr[25];

/*!
  Get version info for current firmware

  \return pointer to version info for this particular firmware
*/
const versionInfo_t *getVersionInfo() {
  return &versionNote.info;
}

/*!
  Find version info in memory (searching the bootloader image, for example)

  \param[in] addr Memory address to start searching in
  \param[in] len Number of bytes to search
  \return none
*/
const versionInfo_t *findVersionInfo(uint32_t addr, uint32_t len) {
  uint32_t *ptr = (uint32_t *)addr;
  const versionInfo_t *rval = NULL;

  // Search memory for the VERSION_MAGIC
  while(ptr < (uint32_t *)(addr + len)) {
    const versionInfo_t *info = (const versionInfo_t *)ptr;
    if(info->magic == VERSION_MAGIC) {
      rval = info;
      break;
    }
    ptr++;
  }

  return rval;

}

/*!
  Is this firmware an engineering build?

  \return true if eng build
*/
bool fwIsEng(const versionInfo_t *info) {
  return (info->flags >> VER_ENG_FLAG_OFFSET) & 0x1;
}

/*!
  Is this firmware taged to the current commit?

  \return true if there have been any changes since tag
*/
bool fwIsDirty(const versionInfo_t *info) {
  return (info->flags >> VER_DIRTY_FLAG_OFFSET) & 0x1;
}

/*!
  Is this firmware taged to the current commit?

  \param[out] *major Pointer to byte for major version number
  \param[out] *minor Pointer to byte for minor version number
  \param[out] *revision Pointer to byte for revision version number
  \return none
*/
void getFWVersion(uint8_t *major, uint8_t *minor, uint8_t *revision) {
  if(major != NULL) {
    *major = versionNote.info.maj;
  }

  if(minor != NULL) {
    *minor = versionNote.info.min;
  }

  if(revision != NULL) {
    *revision = versionNote.info.rev;
  }
}

/*!
  Get device unique hardware id as string

  \return pointer to UID string
*/
const char * getUIDStr() {
  static bool uidStrGenerated = false;
  if (!uidStrGenerated) {
    snprintf(uidStr, sizeof(uidStr), "%08lX%08lX%08lX", UID[2], UID[1], UID[0]);
    uidStrGenerated = true;
  }

  return uidStr;
}

uint32_t getGitSHA() {
  return versionNote.info.gitSHA;
}

/*!
  Get firmware version as string

  \return pointer to firmare version string
*/
const char * getFWVersionStr() {
  static bool versionStrGenerated = false;
  if (!versionStrGenerated) {
    if(fwIsEng(getVersionInfo())) {
      const uint8_t *buildId;
      getBuildId(&buildId);
      snprintf(fwVersionStr, sizeof(fwVersionStr),
      "ENG-%s+%02x%02x%02x%02x",
      versionNote.info.versionStr,
      buildId[0],
      buildId[1],
      buildId[2],
      buildId[3]);
    } else {
      snprintf(fwVersionStr, sizeof(fwVersionStr),
        "%s",
        versionNote.info.versionStr);
    }
    versionStrGenerated = true;
  }

  return fwVersionStr;
}

/*!
  Get pointer to build-id in memory

  \param[out] **buildId Pointer to buildId byte array
  \return size in bytes of build id
*/
size_t getBuildId(const uint8_t **buildId) {
  if(buildId != NULL) {
    *buildId = &g_note_build_id->data[g_note_build_id->namesz];
  }

  return g_note_build_id->descsz;
}
