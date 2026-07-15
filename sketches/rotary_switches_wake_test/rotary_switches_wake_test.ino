// All-inputs sleep/wake test (PCINT)
//
// Single sketch to bring up every GPIO input in the design: all three
// rotary switches, the Resend button, and the Swing toggle. Sleeps in
// SLEEP_MODE_PWR_DOWN and wakes on any edge of any input line via PCINT
// (see 05_electronics_circuit.md §5). On wake, reads all inputs with
// debounce and prints only what changed — nothing is printed while idle.
//
// Wiring (per §1-2 / §4-5):
//   Rotary switches:
//     wiper = +3.3 V
//     each position contact -> diode -> code line (HIGH on selected pos)
//     each code line -> external 1 MΩ pull-down to GND
//     each code line -> one MCU GPIO (INPUT, high-Z; no internal pull-up)
//   Resend button -> external pull-down to GND, button to +3.3V, active-high
//   (bench wiring — same convention as the rotary switch code lines, not the
//   design doc's pull-up/active-low)
//   Swing toggle -> GPIO to GND, pin in INPUT_PULLUP, active-low
//   (per §4 — no external resistor needed)
//
// Pin groups (Arduino labels, per §1 pin table) and their PCINT group:
//   Fan speed (SR16, 8 pos): D10, D11, D12  (PB2/3/4  -> PCINT2/3/4,   PCIE0)
//   Mode      (RS1010, 5 pos): A0, A1, A2   (PC0/1/2  -> PCINT8/9/10,  PCIE1)
//   Temp      (SR16, 8 pos): D4, D5, D6     (PD4/5/6  -> PCINT20/21/22,PCIE2)
//   Resend button: D2 (PD2 / INT0)          (PCINT18, PCIE2)
//   Swing toggle:  D7 (PD7)                 (PCINT23, PCIE2)
// All three PCI groups (0, 1, 2) are armed.
//
// Behaviour:
//   - loop() only sleeps and, on wake, reads + prints — no polling.
//   - Debounces the inter-detent float glitch (all lines briefly read 0):
//     only accepts a code after SETTLE_STABLE_READS consecutive agreeing
//     reads, each separated by SETTLE_MS (per §5 "Debounce").
//   - Prints only on an actual change: switch position, Resend press, or
//     Swing toggle. Silent otherwise, including across the float glitch.

#include <avr/sleep.h>
#include <avr/interrupt.h>

#define BAUD_RATE 115200
#define RESEND_PIN 2   // PD2 / INT0, external pull-down, active-high
#define SWING_PIN  7   // PD7, INPUT_PULLUP, active-low

struct Switch {
    const char *name;
    uint8_t pins[3];
    const uint8_t *rawToPos;  // raw code -> logical position table, or nullptr for identity
    uint8_t lastCode;
};

// Fan speed diode encoding is off by one position: the leftmost detent reads
// 001 (should be 000/Off) and the rightmost reads 000 (should be 111/Auto) —
// every position's code is shifted by one relative to the intended table,
// wrapping mod 8. Net effect: raw code sequence around the knob is
// 0,7,6,5,4,3,2,1 instead of 0,1,2,3,4,5,6,7. Remap here rather than
// re-wiring the diodes. Index = raw code, value = logical position fed to
// fanMeaning().
const uint8_t FAN_RAW_TO_POS[8] = {0, 7, 6, 5, 4, 3, 2, 1};

Switch switches[] = {
    { "Fan",  {10, 11, 12}, FAN_RAW_TO_POS, 0xFF },
    { "Mode", {A2, A1, A0}, nullptr,        0xFF },
    { "Temp", {4,  5,  6},  nullptr,        0xFF },
};
const uint8_t N_SWITCHES = sizeof(switches) / sizeof(switches[0]);
const uint8_t N_BITS = 3;

const uint8_t SETTLE_MS = 10;
const uint8_t SETTLE_STABLE_READS = 5;  // consecutive agreeing reads required
const uint8_t MAX_SETTLE_TRIES = 20;

// Resend / Swing debounce: simple hold-off, not a settle loop like the knobs
// (see 05_electronics_circuit.md §4).
const uint16_t BUTTON_DEBOUNCE_MS = 50;

volatile bool wakeFlag = false;

const char *fanMeaning(uint8_t code) {
    switch (code) {
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

const char *modeMeaning(uint8_t code) {
    switch (code) {
        case 0: return "Fan";
        case 1: return "Cool";
        case 2: return "Heat";
        case 3: return "Dry";
        case 4: return "Auto";
    }
    return "unused";
}

const char *tempMeaning(uint8_t code) {
    // Same 8 positions used for both cooling and heating ranges;
    // print both since mode isn't folded in here (see §2 firmware note).
    static char buf[24];
    int coolC = code * 2 + 20;
    int heatC = code * 2 + 14;
    snprintf(buf, sizeof(buf), "%dC cool / %dC heat", coolC, heatC);
    return buf;
}

void setup() {
    // Clear CKDIV8 so the chip runs at nominal 8 MHz regardless of fuse state
    // (same idiom as daikin_serial / daikin_knob_remote / ir_rx_dump — see
    // howtos/02_serial_debug.md).
    CLKPR = 0x80;
    CLKPR = 0x00;

    Serial.begin(BAUD_RATE);

    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        for (uint8_t i = 0; i < N_BITS; i++) {
            pinMode(switches[s].pins[i], INPUT);
        }
    }
    pinMode(RESEND_PIN, INPUT);  // external pull-down on the bench, not internal pull-up
    pinMode(SWING_PIN, INPUT_PULLUP);  // per §4: internal pull-up, active-low

    // Seed lastCode/lastState from the current wiring so setup doesn't print
    // a spurious "change" for whatever position the knobs happen to rest at.
    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        switches[s].lastCode = readCodeOnce(switches[s].pins);
    }

    enablePCINT();

    Serial.println("All-inputs sleep/wake test ready: Fan/Mode/Temp + Resend + Swing.");
    Serial.println("Sleeping — turn a knob or press a button to see output.");
    Serial.flush();
}

void loop() {
    sleepUntilWake();

    // Woke up: read + debounce every switch, print only what changed.
    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        uint8_t code = readDebouncedCode(switches[s].pins);
        if (code != switches[s].lastCode) {
            printReading(switches[s], code);
            switches[s].lastCode = code;
        }
    }

    if (resendButtonPressed()) {
        Serial.println("* Resend: pressed");
    }

    if (swingChanged()) {
        Serial.print("* Swing: ");
        Serial.println(digitalRead(SWING_PIN) == LOW ? "ON" : "OFF");
    }

    Serial.flush();
}

// --- PCINT: arm every input line, all three PCI groups -----------------

void enablePCINT() {
    cli();
    // Fan: PB2/3/4 -> PCINT2/3/4, group PCIE0.
    PCICR  |= _BV(PCIE0);
    PCMSK0 |= _BV(PCINT2) | _BV(PCINT3) | _BV(PCINT4);

    // Mode: PC0/1/2 -> PCINT8/9/10, group PCIE1.
    PCICR  |= _BV(PCIE1);
    PCMSK1 |= _BV(PCINT8) | _BV(PCINT9) | _BV(PCINT10);

    // Temp: PD4/5/6 -> PCINT20/21/22. Resend: PD2 -> PCINT18.
    // Swing: PD7 -> PCINT23. All group PCIE2.
    PCICR  |= _BV(PCIE2);
    PCMSK2 |= _BV(PCINT20) | _BV(PCINT21) | _BV(PCINT22)
            | _BV(PCINT18) | _BV(PCINT23);
    sei();
}

ISR(PCINT0_vect) { wakeFlag = true; }
ISR(PCINT1_vect) { wakeFlag = true; }
ISR(PCINT2_vect) { wakeFlag = true; }

// Power-down sleep. Wakes on any armed PCINT (any input-line edge,
// including the inter-detent float glitch — that's fine, it only ever
// causes an extra wake, never a missed one).
void sleepUntilWake() {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    wakeFlag = false;
    sleep_enable();
    sei();
    sleep_cpu();
    // Execution resumes here after any PCINTx_vect.
    sleep_disable();
}

// --- Reading -------------------------------------------------------------

uint8_t readCodeOnce(const uint8_t pins[N_BITS]) {
    uint8_t code = 0;
    for (uint8_t i = 0; i < N_BITS; i++) {
        if (digitalRead(pins[i]) == HIGH) {
            code |= (1 << i);
        }
    }
    return code;
}

// SETTLE_STABLE_READS consecutive agreeing reads, each separated by
// SETTLE_MS, defeat both the float-time all-zero glitch and any
// mid-rotation transient (a single lingering transient can still fool a
// 2-read check, so require a longer run of agreement).
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

// Active-high (external pull-down on the bench — button to +3.3V, not GND).
// Simple debounce: require the line to still read pressed after
// BUTTON_DEBOUNCE_MS, then wait for release before returning true again
// (edge-triggered, not level-triggered).
bool resendButtonPressed() {
    static bool wasPressed = false;
    bool isPressed = (digitalRead(RESEND_PIN) == HIGH);

    if (isPressed && !wasPressed) {
        delay(BUTTON_DEBOUNCE_MS);
        isPressed = (digitalRead(RESEND_PIN) == HIGH);
    }

    bool triggered = isPressed && !wasPressed;
    wasPressed = isPressed;
    return triggered;
}

// INPUT_PULLUP, active-low (per §4: toggle to GND, no external resistor).
// Simple level debounce: report a change only after it holds for
// BUTTON_DEBOUNCE_MS.
bool swingChanged() {
    static bool lastState = HIGH;
    bool state = digitalRead(SWING_PIN);

    if (state != lastState) {
        delay(BUTTON_DEBOUNCE_MS);
        state = digitalRead(SWING_PIN);
        if (state == lastState) return false;
        lastState = state;
        return true;
    }
    return false;
}

void printReading(const Switch &sw, uint8_t code) {
    Serial.print("* ");
    Serial.print(sw.name);
    Serial.print(": ");
    for (int i = N_BITS - 1; i >= 0; i--) {
        Serial.print("b");
        Serial.print(i);
        Serial.print("=");
        Serial.print((code >> i) & 1);
        Serial.print(" ");
    }
    Serial.print(" code=");
    Serial.print(code);
    uint8_t pos = sw.rawToPos ? sw.rawToPos[code] : code;
    if (sw.rawToPos) {
        Serial.print(" pos=");
        Serial.print(pos);
    }
    Serial.print("  (");
    if (sw.name[0] == 'F') Serial.print(fanMeaning(pos));
    else if (sw.name[0] == 'M') Serial.print(modeMeaning(pos));
    else Serial.print(tempMeaning(pos));
    Serial.println(")");
}
