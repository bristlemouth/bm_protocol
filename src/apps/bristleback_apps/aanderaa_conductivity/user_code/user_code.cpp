#include "user_code.h"

// Global sensor app instance
SensorAppUser app;

void setup(void) {
    app.setup_with_pins(&BB_VBUS_EN, &BB_PL_BUCK_EN, 500);
}

void loop(void) {
    app.loop();
}
