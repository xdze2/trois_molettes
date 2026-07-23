#pragma once
#include <stdint.h>

// Mode constants (match IRremoteESP8266 / Daikin protocol)
#define DAIKIN_MODE_AUTO  0b000
#define DAIKIN_MODE_DRY   0b010
#define DAIKIN_MODE_COOL  0b011
#define DAIKIN_MODE_HEAT  0b100
#define DAIKIN_MODE_FAN   0b110

// Fan speed constants
#define DAIKIN_FAN_1     3   // min  (wire value = user_value + 2)
#define DAIKIN_FAN_2     4
#define DAIKIN_FAN_3     5   // med
#define DAIKIN_FAN_4     6
#define DAIKIN_FAN_5     7   // max
#define DAIKIN_FAN_AUTO  0b1010
#define DAIKIN_FAN_QUIET 0b1011

#define DAIKIN_FRAME_LEN 35

struct ACState {
    bool    power;
    uint8_t mode;     // DAIKIN_MODE_*
    uint8_t temp;     // °C, clamped to 10–32 in daikin_build_frame()
    uint8_t fan;      // DAIKIN_FAN_*
    bool    swing_v;
    bool    powerful;
    bool    econo;
};

// Fills frame[35] with the complete Daikin base-protocol message,
// including fixed header bytes and all three checksums.
void daikin_build_frame(const ACState *state, uint8_t frame[DAIKIN_FRAME_LEN]);
