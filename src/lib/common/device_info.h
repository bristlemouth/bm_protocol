#pragma once

#include <stdbool.h>
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

const versionInfo_t *getVersionInfo();
const versionInfo_t *findVersionInfo(uint32_t addr, uint32_t len);

bool fwIsEng(const versionInfo_t *info);
bool fwIsDirty(const versionInfo_t *info);
const uint32_t* getUID();
const char * getUIDStr();
const char * getFWVersionStr();
uint32_t getGitSHA();
void getFWVersion(uint8_t *major, uint8_t *minor, uint8_t *revision);
size_t getBuildId(const uint8_t **buildId);
void getMacAddr(uint8_t *buff, size_t len);
uint64_t getNodeId();
#ifdef __cplusplus
}
#endif
