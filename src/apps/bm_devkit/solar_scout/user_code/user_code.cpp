#include "user_code.h"
#include "debug_solar_scout.h"
#include "solar_scout_comms.h"
#include "spotter.h"

void setup(void) {
  solar_scout_comms_init();
  debug_solar_scout_init();
}

void loop(void) { /* USER LOOP CODE GOES HERE */ }
