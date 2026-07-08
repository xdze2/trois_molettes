// All-3-switches polling test (no sleep)
//
// Reads the diode-encoded code lines of all three rotary switches
// (see 05_electronics_circuit.md §1-2) and prints each switch's bits,
// decoded position, and (where known) meaning on Serial.
//
// Wiring (per §1-2 / §5):
//   wiper = +3.3 V
//   each position contact -> diode -> code line (HIGH on selected pos)
//   each code line -> external 1 MΩ pull-down to GND
//   each code line -> one MCU GPIO (INPUT, high-Z; no internal pull-up)
//
// Pin groups (Arduino labels, per §1 pin table):
//   Fan speed (SR16, 8 pos): D10, D11, D12  (PB2, PB3, PB4)
//   Mode      (RS1010, 5 pos): A0, A1, A2   (PC0, PC1, PC2)
//   Temp      (SR16, 8 pos): D4, D5, D6     (PD4, PD5, PD6)
//
// Behaviour:
//   - Pure polling loop, no sleep, no IRQ.
//   - Debounces the inter-detent float glitch (all lines briefly read 0):
//     only accepts a code after two consecutive reads, separated by
//     SETTLE_MS, agree (per §5 "Debounce").
//   - Prints on any switch change and at PRINT_INTERVAL_MS heartbeat.

#define BAUD_RATE 9600

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

const unsigned long PRINT_INTERVAL_MS = 5000;
const uint8_t SETTLE_MS = 10;
const uint8_t SETTLE_STABLE_READS = 5;  // consecutive agreeing reads required
const uint8_t MAX_SETTLE_TRIES = 20;

unsigned long lastPrint = 0;

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
    Serial.begin(BAUD_RATE);

    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        for (uint8_t i = 0; i < N_BITS; i++) {
            pinMode(switches[s].pins[i], INPUT);
        }
    }

    Serial.println("Rotary switches (Fan/Mode/Temp) polling test ready.");
    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        Serial.print(switches[s].name);
        Serial.print(" pins (b0..b2): ");
        for (uint8_t i = 0; i < N_BITS; i++) {
            Serial.print(switches[s].pins[i]);
            if (i < N_BITS - 1) Serial.print(", ");
        }
        Serial.println();
    }
}

void loop() {
    unsigned long now = millis();
    bool tick = (now - lastPrint >= PRINT_INTERVAL_MS);
    bool anyChanged = false;

    uint8_t codes[N_SWITCHES];
    bool changed[N_SWITCHES];

    for (uint8_t s = 0; s < N_SWITCHES; s++) {
        codes[s] = readDebouncedCode(switches[s].pins);
        changed[s] = (codes[s] != switches[s].lastCode);
        anyChanged |= changed[s];
    }

    if (anyChanged || tick) {
        for (uint8_t s = 0; s < N_SWITCHES; s++) {
            printReading(switches[s], codes[s], changed[s]);
            switches[s].lastCode = codes[s];
        }
        Serial.println();
        lastPrint = now;
    }
}

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

void printReading(const Switch &sw, uint8_t code, bool changed) {
    Serial.print(changed ? "* " : "  ");
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
