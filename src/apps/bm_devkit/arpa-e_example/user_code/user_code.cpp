#include "user_code.h"
#include "bq25820.h"
#include "ina232.h"

static BQ::BQ25820 Charger(&i2c1);

void setup(void) {
  // Turn on the VBUS load switch on the mote
  IOWrite(&VBUS_EN, 0);
  // Wait before trying to initialize the battery charger IC
  vTaskDelay(pdMS_TO_TICKS(20));
  // Try to initialize the charger IC
  if(Charger.init()) {
      printf("Charger IC Initialized\n");
			printf("Disabling pulse frequency modulation on the charger.\n");
      Charger.disablePfm();
  }
  else {
      printf("Failed to init the charger IC.\n");
  }
  /* USER ONE-TIME SETUP CODE GOES HERE */

}

void loop(void) { /* USER LOOP CODE GOES HERE */ }
