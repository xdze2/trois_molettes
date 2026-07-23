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
//   Temp      (SR16, 8 pos):  D6, D5, D4     (PD6, PD5, PD4 — note swapped
//                                              b0/b2 pins vs. the design doc;
//                                              see howto 09)
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
// Knob configuration — one declarative table per knob
// ===========================================================================
//
// The complete knob mapping lives here and NOWHERE else. Each knob is ONE
// table, indexed directly by the raw gpio reading — row N is the meaning of
// raw code N. The human-readable `label` and the value written into ACState
// live in the SAME row, so they can never drift out of sync, and there is no
// separate raw->position remap table to keep in step (the old code split this
// across fanMeaning()/modeMeaning(), applyFanPos()/applyModePos(), AND a
// FAN_RAW_TO_POS array — three things to keep aligned by hand).
//
// The rows are therefore ordered by *raw code*, not by panel position — so any
// diode shift / reversed rotation / pin swap is absorbed directly into the row
// order here. To verify a knob: turn it left->right, watch the `raw=` values on
// the CHANGE line, and confirm FAN_POS[raw] names what the panel says.

// --- Fan knob (SR16, 8 positions), indexed by raw gpio code ---------------
// power=false marks the Off detent (fan value is then don't-care).
// Row order set from the bench sweep (howto 09): the panel's leftmost detent
// (Off) reads raw=1 and its rightmost (Auto) reads raw=0 — a wiring error, but
// absorbed here so the rows match the panel by raw code. See per-row notes.
struct FanPos { const char *label; bool power; uint8_t fan; };
// IMPORTANT: rows are indexed by raw code — FAN_POS[raw] must be raw's meaning,
// so the row order here MUST stay sorted by the /* raw N */ tag (0,1,2,...).
// Do not reorder into panel order; that breaks the direct-index contract.
const FanPos FAN_POS[8] = {
    { "Auto",    true,  DAIKIN_FAN_AUTO  },  // right most, wiring error
    { "Off",     false, DAIKIN_FAN_AUTO  },  // leftmost — confirmed
    { "Speed 1", true,  DAIKIN_FAN_1     },  //
    { "Speed 2", true,  DAIKIN_FAN_2     },  //
    { "Speed 3", true,  DAIKIN_FAN_3     },  //
    { "Speed 4", true,  DAIKIN_FAN_4     },  // 
    { "Speed 5", true,  DAIKIN_FAN_5     },  // wiring error... 
    { "Quiet",   true,  DAIKIN_FAN_QUIET },  //
};

// --- Mode knob (RS1010, 5 positions), indexed by raw gpio code ------------
// Bench-validated: the knob reads reversed vs. the old howto 09 order, so by
// raw code raw 0..4 = Auto/Dry/Heat/Cool/Fan (rows below). Re-confirm against
// the panel with the raw= reading on the CHANGE line if the wiring changes.
struct ModePos { const char *label; uint8_t mode; };
const ModePos MODE_POS[5] = {
    { "Auto", DAIKIN_MODE_AUTO },
    { "Dry",  DAIKIN_MODE_DRY  },
    { "Heat", DAIKIN_MODE_HEAT },
    { "Cool", DAIKIN_MODE_COOL },
    { "Fan",  DAIKIN_MODE_FAN  },
};

// --- Temp knob (SR16, 8 positions), left -> right -------------------------
// Position -> degrees C is a formula, not a table: mode-dependent base + 2*pos
// (Heat 14-28C, else 20-34C; 00_specifications.md 4.3). See applyTempPos().
const uint8_t N_TEMP_POS = 8;

const uint8_t N_FAN_POS  = sizeof(FAN_POS)  / sizeof(FAN_POS[0]);
const uint8_t N_MODE_POS = sizeof(MODE_POS) / sizeof(MODE_POS[0]);

// ---------------------------------------------------------------------------
// Rotary switches — pins + last-seen raw code. There is no raw->position
// remap here anymore: the per-knob POS tables above are already ordered by raw
// code, so the raw gpio reading indexes them directly. Any diode shift / pin
// swap / reversal is absorbed into that row order. See howto 09.
// ---------------------------------------------------------------------------
struct Switch {
    const char *name;
    uint8_t pins[3];
    uint8_t lastCode;
};

enum { SW_FAN = 0, SW_MODE = 1, SW_TEMP = 2 };

Switch switches[] = {
    { "Fan",  {10, 11, 12}, 0xFF },
    { "Mode", {A2, A1, A0}, 0xFF },  // pin order per howto 09 fix
    { "Temp", {6,  5,  4},  0xFF },  // b0/b2 pins swapped vs. design doc; see howto 09
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
// Switch reading — verbatim pattern from rotary_switches_wake_test.ino
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
// Position -> Daikin protocol value mapping — thin lookups into the per-knob
// tables above. All the actual mapping knowledge lives in FAN_POS / MODE_POS.
// ---------------------------------------------------------------------------
const char *fanMeaning(uint8_t pos) {
    return (pos < N_FAN_POS) ? FAN_POS[pos].label : "?";
}

const char *modeMeaning(uint8_t pos) {
    return (pos < N_MODE_POS) ? MODE_POS[pos].label : "?";
}

// One knob's contribution to the "CHANGE" line, e.g. "  Mode raw=4 (Auto)".
// raw is the gpio reading, which is also the index into the POS tables.
void printKnobChange(uint8_t s, uint8_t raw) {
    Serial.print("  ");
    Serial.print(switches[s].name);
    Serial.print(" raw=");
    Serial.print(raw);
    if (s == SW_FAN || s == SW_MODE) {
        Serial.print(" (");
        Serial.print(s == SW_FAN ? fanMeaning(raw) : modeMeaning(raw));
        Serial.print(")");
    }
}

// Fan knob position (0..7, 00_specifications.md 4.1) -> ACState fields.
void applyFanPos(uint8_t pos, ACState *st) {
    if (pos >= N_FAN_POS) pos = 1;  // out-of-range -> Off (index 1), fail safe
    st->power = FAN_POS[pos].power;
    st->fan   = FAN_POS[pos].fan;
}

// Mode knob position (0..4, 00_specifications.md 4.2) -> ACState.mode.
void applyModePos(uint8_t pos, ACState *st) {
    if (pos >= N_MODE_POS) pos = 0;  // out-of-range -> Auto (index 0), fail safe
    st->mode = MODE_POS[pos].mode;
}

// Temp knob raw code (0..7, 05_electronics_circuit.md §2) -> degrees C.
// Mode-dependent offset: Heat uses 14-28C, everything else uses 20-34C,
// both in 2C steps (00_specifications.md 4.3).
//
// Unlike Fan/Mode, Temp is a formula not a table, so a shifted or reversed
// Temp knob can't be absorbed into row order — it must be corrected here.
// Temp is not yet bench-verified (see howto 09 "Next"); if the sweep shows it
// reversed, map raw->pos first, e.g.  pos = (N_TEMP_POS - 1) - raw;
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
    uint8_t raw[N_SWITCHES];
    bool    changed[N_SWITCHES];

    // First: read every knob, recording which ones moved. The raw gpio code
    // *is* the index into the POS tables, so there's no separate decode step.
    // Print nothing yet — the "what changed" line (below) is emitted once,
    // listing only the moved knobs with their raw reading.
    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        raw[s] = readDebouncedCode(switches[s].pins);
        changed[s] = (raw[s] != switches[s].lastCode);
        if (changed[s]) {
            switches[s].lastCode = raw[s];
            anyChanged = true;
        }
    }

    if (anyChanged) {
        // Line 1: what changed (with raw gpio reading). Only moved knobs listed.
        Serial.print("CHANGE");
        for (uint8_t s = 0; s < N_SWITCHES; s++) {
            if (changed[s]) printKnobChange(s, raw[s]);
        }
        Serial.println();
        // Lines 2 & 3: what is sent, then raw bytes.
        sendCurrentState(raw[SW_FAN], raw[SW_MODE], raw[SW_TEMP]);
    } else if (resendButtonPressed()) {
        // Resend: retransmit current knob state unchanged, without requiring
        // a knob move (00_specifications.md "resend action" /
        // 01_IR_protocol_and_mapping.md RESEND).
        Serial.println("CHANGE (resend)");
        sendCurrentState(raw[SW_FAN], raw[SW_MODE], raw[SW_TEMP]);
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
