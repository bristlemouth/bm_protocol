#pragma once

#include "version.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

const versionInfo_t *getVersionInfo(void);
const versionInfo_t *findVersionInfo(uint32_t addr, uint32_t len);

bool fwIsEng(const versionInfo_t *info);
bool fwIsDirty(const versionInfo_t *info);
const uint32_t *getUID(void);
const char *getUIDStr(void);
const char *getFWVersionStr(void);
uint32_t getGitSHA(void);
void getFWVersion(uint8_t *major, uint8_t *minor, uint8_t *revision);
size_t getBuildId(const uint8_t **buildId);
void getMacAddr(uint8_t *buff, size_t len);
uint64_t getNodeId(void);
const char *getNodeIdStr(void);
void setHwVersion(uint8_t version);
uint8_t getHwVersion(void);
#ifdef __cplusplus
}
#endif
