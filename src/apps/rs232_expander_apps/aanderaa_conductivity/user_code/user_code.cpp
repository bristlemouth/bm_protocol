#include "user_code.h"

// Global sensor app instance
SensorAppUser app;

void setup(void) {
    app.setup_with_pins(&VBUS_EN, &PL_BUCK_EN, 500);
}

void loop(void) {
    app.loop();
}
