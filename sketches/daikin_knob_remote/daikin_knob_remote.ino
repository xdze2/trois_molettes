// daikin_knob_remote — ATmega328P Pro Mini @ 3.3V, 8 MHz
//
// Near-final knob firmware: reads the Fan, Mode and Temp rotary switches and
// transmits a Daikin IR frame built from the current knob state on every
// change of any knob (no Send button — the switch position is the state,
// per 11_serial_remote_app.md's stateless-device model). A Resend button
// re-transmits the current state on demand without requiring a knob change
// (per 00_specifications.md §"resend action" / 01_IR_protocol_and_mapping.md
// RESEND). Logs every reading and every transmit to Serial.
//
// Pure polling, no sleep, no IRQ — matches the bring-up sketches this is
// built from. Sleep + PCINT wake is a separate, later step (see howto 09).
//
// Wiring (per 05_electronics_circuit.md §1, confirmed in howto 09):
//   Fan speed (SR16, 8 pos):  D10, D11, D12  (PB2, PB3, PB4)
//   Mode      (RS1010, 5 pos): A2, A1, A0    (PC2, PC1, PC0 — note swapped
//                                              order vs. the design doc; see
//                                              howto 09 "Mode: pins were
//                                              swapped, not the switch")
//   Temp      (SR16, 8 pos):  D4, D5, D6     (PD4, PD5, PD6)
//   Resend button:             D2 (PD2, INT0), external pull-down, active-high
//                               (bench wiring: button to +3.3V, external R to
//                               GND — same convention as the rotary switch
//                               code lines, not the doc's pull-up/active-low)
//
// IR path — Timer2 38 kHz carrier on D3 (OC2B), verbatim from daikin_serial /
// daikin_fan_toggle. daikin_build_frame() / firmware protocol timing are the
// validated, shared source (firmware/daikin_frame.{h,cpp}, symlinked here).

#include <avr/io.h>
#include <string.h>

#include "daikin_frame.h"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
#define BAUD_RATE 115200
#define IR_PIN     3   // OC2B — Timer2 compare output B
#define RESEND_PIN 2   // PD2 / INT0, external pull-down, active-high

// ---------------------------------------------------------------------------
// Protocol timing constants (µs) — identical to daikin_serial / daikin_fan_toggle
// ---------------------------------------------------------------------------
#define DAIKIN_HDR_MARK    3500
#define DAIKIN_HDR_SPACE   1700
#define DAIKIN_BIT_MARK     428
#define DAIKIN_ONE_SPACE   1280
#define DAIKIN_ZERO_SPACE   428
#define DAIKIN_LEADER_GAP 25000
#define DAIKIN_SECTION_GAP 35000

// ---------------------------------------------------------------------------
// Rotary switches — Fan + Mode (see howtos/09_rotary_switches_poll_test.md)
// ---------------------------------------------------------------------------
struct Switch {
    const char *name;
    uint8_t pins[3];
    const uint8_t *rawToPos;  // raw code -> logical position table, or nullptr for identity
    uint8_t lastCode;
};

// Fan diode encoding is shifted by one position; remap raw code -> logical
// position rather than re-wiring the diodes. See howto 09.
const uint8_t FAN_RAW_TO_POS[8] = {0, 7, 6, 5, 4, 3, 2, 1};

enum { SW_FAN = 0, SW_MODE = 1, SW_TEMP = 2 };

Switch switches[] = {
    { "Fan",  {10, 11, 12}, FAN_RAW_TO_POS, 0xFF },
    { "Mode", {A2, A1, A0}, nullptr,        0xFF },  // pin order per howto 09 fix
    { "Temp", {4,  5,  6},  nullptr,        0xFF },
};
const uint8_t N_SWITCHES = sizeof(switches) / sizeof(switches[0]);
const uint8_t N_BITS = 3;

const uint8_t SETTLE_MS = 10;
const uint8_t SETTLE_STABLE_READS = 5;  // consecutive agreeing reads required
const uint8_t MAX_SETTLE_TRIES = 20;

// Resend button debounce (simple hold-off, not a settle loop like the knobs —
// see 05_electronics_circuit.md §4).
const uint16_t RESEND_DEBOUNCE_MS = 50;

// ---------------------------------------------------------------------------
// Timer2 -- 38 kHz carrier on OC2B (D3)
// ---------------------------------------------------------------------------
static void timer2_38khz_start() {
    TCCR2A = (1 << COM2B0) | (1 << WGM21);
    TCCR2B = (1 << CS20);
    OCR2A  = 104;
    TCNT2  = 0;
    pinMode(IR_PIN, OUTPUT);
}

// ---------------------------------------------------------------------------
// mark / space
// ---------------------------------------------------------------------------
static void ir_mark(uint16_t us) {
    TCCR2A |= (1 << COM2B0);
    delayMicroseconds(us);
}

static void ir_space(uint16_t us) {
    TCCR2A &= ~(1 << COM2B0);
    digitalWrite(IR_PIN, LOW);
    delayMicroseconds(us);
}

static void ir_space_long(uint32_t us) {
    TCCR2A &= ~(1 << COM2B0);
    digitalWrite(IR_PIN, LOW);
    while (us >= 16000) { delay(16); us -= 16000; }
    if (us) delayMicroseconds((uint16_t)us);
}

// ---------------------------------------------------------------------------
// send_byte / send_section / send_daikin — verbatim from daikin_serial
// ---------------------------------------------------------------------------
static void send_byte(uint8_t b) {
    for (uint8_t i = 0; i < 8; i++) {
        ir_mark(DAIKIN_BIT_MARK);
        if (b & 1)
            ir_space(DAIKIN_ONE_SPACE);
        else
            ir_space(DAIKIN_ZERO_SPACE);
        b >>= 1;
    }
}

static void send_section(const uint8_t *data, uint8_t len, uint32_t gap) {
    ir_mark(DAIKIN_HDR_MARK);
    ir_space(DAIKIN_HDR_SPACE);
    for (uint8_t i = 0; i < len; i++) send_byte(data[i]);
    ir_mark(DAIKIN_BIT_MARK);
    ir_space_long(gap);
}

static void send_daikin(const uint8_t frame[DAIKIN_FRAME_LEN]) {
    for (uint8_t i = 0; i < 5; i++) {
        ir_mark(DAIKIN_BIT_MARK);
        ir_space(DAIKIN_ZERO_SPACE);
    }
    ir_mark(DAIKIN_BIT_MARK);
    ir_space_long(DAIKIN_LEADER_GAP);

    send_section(frame,      8,  DAIKIN_SECTION_GAP);
    send_section(frame + 8,  8,  DAIKIN_SECTION_GAP);
    send_section(frame + 16, 19, DAIKIN_SECTION_GAP);
}

// ---------------------------------------------------------------------------
// Switch reading — verbatim pattern from rotary_switches_poll_test.ino
// ---------------------------------------------------------------------------
uint8_t readCodeOnce(const uint8_t pins[N_BITS]) {
    uint8_t code = 0;
    for (uint8_t i = 0; i < N_BITS; i++) {
        if (digitalRead(pins[i]) == HIGH) {
            code |= (1 << i);
        }
    }
    return code;
}

uint8_t readDebouncedCode(const uint8_t pins[N_BITS]) {
    uint8_t stable = readCodeOnce(pins);
    uint8_t stableCount = 1;
    for (uint8_t i = 0; i < MAX_SETTLE_TRIES; i++) {
        delay(SETTLE_MS);
        uint8_t cur = readCodeOnce(pins);
        if (cur == stable) {
            stableCount++;
            if (stableCount >= SETTLE_STABLE_READS) return stable;
        } else {
            stable = cur;
            stableCount = 1;
        }
    }
    return stable;
}

// ---------------------------------------------------------------------------
// Position -> Daikin protocol value mapping
// ---------------------------------------------------------------------------
const char *fanMeaning(uint8_t pos) {
    switch (pos) {
        case 0: return "Off";
        case 1: return "Speed 1";
        case 2: return "Speed 2";
        case 3: return "Speed 3";
        case 4: return "Speed 4";
        case 5: return "Speed 5";
        case 6: return "Quiet";
        case 7: return "Auto";
    }
    return "?";
}

const char *modeMeaning(uint8_t pos) {
    switch (pos) {
        case 0: return "Fan";
        case 1: return "Cool";
        case 2: return "Heat";
        case 3: return "Dry";
        case 4: return "Auto";
    }
    return "?";
}

// Fan knob position (0..7, 00_specifications.md 4.1) -> ACState fields.
// Position 0 is Off (power=false); fan value is don't-care when off.
void applyFanPos(uint8_t pos, ACState *st) {
    if (pos == 0) {
        st->power = false;
        st->fan   = DAIKIN_FAN_AUTO;
        return;
    }
    st->power = true;
    if (pos == 6)      st->fan = DAIKIN_FAN_QUIET;
    else if (pos == 7) st->fan = DAIKIN_FAN_AUTO;
    else                st->fan = (uint8_t)(pos + 2);  // 1..5 -> wire 3..7
}

// Mode knob position (0..4, 00_specifications.md 4.2) -> ACState.mode.
void applyModePos(uint8_t pos, ACState *st) {
    switch (pos) {
        case 0: st->mode = DAIKIN_MODE_FAN;  break;
        case 1: st->mode = DAIKIN_MODE_COOL; break;
        case 2: st->mode = DAIKIN_MODE_HEAT; break;
        case 3: st->mode = DAIKIN_MODE_DRY;  break;
        case 4: st->mode = DAIKIN_MODE_AUTO; break;
        default: st->mode = DAIKIN_MODE_AUTO; break;
    }
}

// Temp knob position (0..7, 05_electronics_circuit.md §2) -> degrees C.
// Mode-dependent offset: Heat uses 14-28C, everything else uses 20-34C,
// both in 2C steps (00_specifications.md 4.3).
uint8_t applyTempPos(uint8_t pos, uint8_t mode) {
    uint8_t base = (mode == DAIKIN_MODE_HEAT) ? 14 : 20;
    return base + pos * 2;
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------
void setup() {
    // Clear CKDIV8 so the chip runs at nominal 8 MHz regardless of fuse state
    // (same idiom as ir_rx_dump / daikin_serial).
    CLKPR = 0x80;
    CLKPR = 0x00;

    Serial.begin(BAUD_RATE);

    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        for (uint8_t i = 0; i < N_BITS; i++) {
            pinMode(switches[s].pins[i], INPUT);
        }
    }
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(RESEND_PIN, INPUT);  // external pull-down on the bench, not internal pull-up

    timer2_38khz_start();

    Serial.println("READY daikin-knob-remote/1.0");
}

void loop() {
    // Poll Fan + Mode + Temp; transmit whenever any changes (knob position
    // *is* the state — no separate Send button, per 11_serial_remote_app.md's
    // stateless-device model).
    bool anyChanged = false;
    uint8_t pos[N_SWITCHES];

    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        uint8_t code = readDebouncedCode(switches[s].pins);
        pos[s] = switches[s].rawToPos ? switches[s].rawToPos[code] : code;
        if (pos[s] != switches[s].lastCode) {
            Serial.print(switches[s].name);
            Serial.print(": pos=");
            Serial.print(pos[s]);
            if (s == SW_FAN || s == SW_MODE) {
                Serial.print(" (");
                Serial.print(s == SW_FAN ? fanMeaning(pos[s]) : modeMeaning(pos[s]));
                Serial.print(")");
            }
            Serial.println();
            switches[s].lastCode = pos[s];
            anyChanged = true;
        }
    }

    if (anyChanged) {
        sendCurrentState(pos[SW_FAN], pos[SW_MODE], pos[SW_TEMP]);
    } else if (resendButtonPressed()) {
        // Resend: retransmit current knob state unchanged, without requiring
        // a knob move (00_specifications.md "resend action" /
        // 01_IR_protocol_and_mapping.md RESEND).
        Serial.println("RESEND");
        sendCurrentState(pos[SW_FAN], pos[SW_MODE], pos[SW_TEMP]);
    }
}

// Active-high (external pull-down on the bench — button to +3.3V, not GND).
// Simple debounce: require the line to still read pressed after
// RESEND_DEBOUNCE_MS, then wait for release before returning true again
// (edge-triggered, not level-triggered).
bool resendButtonPressed() {
    static bool wasPressed = false;
    bool isPressed = (digitalRead(RESEND_PIN) == HIGH);

    if (isPressed && !wasPressed) {
        delay(RESEND_DEBOUNCE_MS);
        isPressed = (digitalRead(RESEND_PIN) == HIGH);
    }

    bool triggered = isPressed && !wasPressed;
    wasPressed = isPressed;
    return triggered;
}

void sendCurrentState(uint8_t fanPos, uint8_t modePos, uint8_t tempPos) {
    ACState st;
    memset(&st, 0, sizeof(st));
    applyFanPos(fanPos, &st);
    applyModePos(modePos, &st);
    st.temp    = applyTempPos(tempPos, st.mode);
    st.swing_v = false;

    Serial.print("SEND fan=");
    Serial.print(fanMeaning(fanPos));
    Serial.print(" mode=");
    Serial.print(modeMeaning(modePos));
    Serial.print(" temp=");
    Serial.print(st.temp);
    Serial.println();

    uint8_t frame[DAIKIN_FRAME_LEN];
    daikin_build_frame(&st, frame);

    digitalWrite(LED_BUILTIN, HIGH);
    send_daikin(frame);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.print("SENT ");
    for (uint8_t i = 0; i < DAIKIN_FRAME_LEN; i++) {
        if (frame[i] < 0x10) Serial.print('0');
        Serial.print(frame[i], HEX);
    }
    Serial.println();
}
