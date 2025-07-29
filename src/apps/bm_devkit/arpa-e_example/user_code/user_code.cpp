#include "user_code.h"
#include "bq25820.h"
#include "bsp.h"
#include "debug.h"
#include "ina232.h"
#include "string.h"

static BQ::BQ25820 Charger(&i2c1);

void setup(void) {
  // Turn on the VBUS load switch on the mote
  IOWrite(&VBUS_BF_EN, 0);
  // Wait before trying to initialize the battery charger IC
  vTaskDelay(pdMS_TO_TICKS(20));
  // Try to initialize the charger IC
  if (Charger.init()) {
    printf("Charger IC Initialized\n");
    printf("Disabling pulse frequency modulation on the charger.\n");
    Charger.disablePfm();
  } else {
    printf("Failed to init the charger IC.\n");
  }
  /* USER ONE-TIME SETUP CODE GOES HERE */
}

void loop(void) { 
  char cdata[1024];
  memset(cdata, 0, sizeof(cdata));

  char cfaults[1024];
  memset(cfaults, 0, sizeof(cfaults));

  Charger.printSensors(cdata, sizeof(cdata));
  Charger.printFaults(cfaults, sizeof(cfaults));

  printf("%s", cdata);
  printf("%s\n", cfaults);
  
  /* USER LOOP CODE GOES HERE */
}
