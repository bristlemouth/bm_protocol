#ifndef __SOLAR_SCOUT_COMMS_H__
#define __SOLAR_SCOUT_COMMS_H__

#include "sss.h"
#include "util.h"

BmErr solar_scout_comms_init(void);
BmErr solar_scout_send_cmd(SSSCommand cmd, uint8_t *payload, uint16_t len);

#endif
