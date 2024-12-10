#include "device_info.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include <string.h>
// #include "stm32l4xx_hal.h"
#include "fnv.h"
#include "stm32u5xx_hal.h"

extern uint8_t __start_gnu_build_id_start[];
const ElfNoteSection_t *g_note_build_id = (ElfNoteSection_t *)__start_gnu_build_id_start;
const uint32_t *UID = (const uint32_t *)(UID_BASE); // 96-bit unique hardware id

static char fwVersionStr[128];
static char uidStr[25];
static char nodeidStr[17];
static uint8_t hwVersion = 0;

/*!
  Get version info for current firmware

  \return pointer to version info for this particular firmware
*/
const versionInfo_t *getVersionInfo(void) { return &versionNote.info; }

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
  while (ptr < (uint32_t *)(addr + len)) {
    const versionInfo_t *info = (const versionInfo_t *)ptr;
    if (info->magic == VERSION_MAGIC) {
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
bool fwIsEng(const versionInfo_t *info) { return (info->flags >> VER_ENG_FLAG_OFFSET) & 0x1; }

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
  if (major != NULL) {
    *major = versionNote.info.maj;
  }

  if (minor != NULL) {
    *minor = versionNote.info.min;
  }

  if (revision != NULL) {
    *revision = versionNote.info.rev;
  }
}

/*!
  Get device unique hardware id as string

  \return pointer to UID string
*/
const char *getUIDStr(void) {
  static bool uidStrGenerated = false;
  if (!uidStrGenerated) {
    snprintf(uidStr, sizeof(uidStr), "%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "", UID[2], UID[1],
             UID[0]);
    uidStrGenerated = true;
  }

  return uidStr;
}

/*!
  Get device unique hardware id

  \return pointer to UID
*/
const uint32_t *getUID(void) { return UID; }

uint32_t getGitSHA() { return versionNote.info.gitSHA; }

/*!
  Get firmware version as string

  \return pointer to firmare version string
*/
const char *getFWVersionStr(void) {
  static bool versionStrGenerated = false;
  if (!versionStrGenerated) {
    if (fwIsEng(getVersionInfo())) {
      const uint8_t *buildId;
      getBuildId(&buildId);
      snprintf(fwVersionStr, sizeof(fwVersionStr), "%s@ENG-%s+%02x%02x%02x%02x", APP_NAME,
               versionNote.info.versionStr, buildId[0], buildId[1], buildId[2], buildId[3]);
    } else {
      snprintf(fwVersionStr, sizeof(fwVersionStr), "%s@%s", APP_NAME,
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
  if (buildId != NULL) {
    *buildId = &g_note_build_id->data[g_note_build_id->namesz];
  }

  return g_note_build_id->descsz;
}

/*!
  Generate 48-bit MAC address from device's node_id.
  First 16 bits are common to all BM devices (0's for now)
  Last 32 bits the 32 least significant bits of the node id

  \param[out] *buff - 6 byte buffer to store MAC address
  \param[in] size of buffer as a safety check (must be >=)
*/
void getMacAddr(uint8_t *buff, size_t len) {
  // MAC address is exactly 48 bits/6 bytes
  configASSERT(len >= 6);

  uint64_t node_id = getNodeId();
  buff[0] = 0x00; // TODO -
  buff[1] = 0x00;
  buff[2] = (node_id >> 24) & 0xFF;
  buff[3] = (node_id >> 16) & 0xFF;
  buff[4] = (node_id >> 8) & 0xFF;
  buff[5] = (node_id >> 0) & 0xFF;
}

uint64_t getNodeId(void) {
  static uint64_t *node_id;

  // Only compute the hash the first time this function gets called
  if (node_id == NULL) {
    node_id = (uint64_t *)pvPortMalloc(sizeof(uint64_t));
    configASSERT(node_id);
    *node_id = fnv_64a_buf((void *)UID, sizeof(uint32_t) * 3, 0);
  }

  return *node_id;
}

/*!
  Get device node id id as string

  \return pointer to node id string
*/
const char *getNodeIdStr(void) {
  static bool nodeidStrGenerated = false;
  if (!nodeidStrGenerated) {
    snprintf(nodeidStr, sizeof(nodeidStr), "%016" PRIx64 "", getNodeId());
    nodeidStrGenerated = true;
  }

  return nodeidStr;
}

void setHwVersion(uint8_t version) { hwVersion = version; }

uint8_t getHwVersion(void) { return hwVersion; }
