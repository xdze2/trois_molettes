// RS1010 polling test (no sleep)
//
// Reads 3 GPIOs wired to the diode-encoded code lines of an RS1010
// rotary switch (see 05_electronics_circuit.md §3.5) and prints the
// per-line bits plus the decoded code on Serial.
//
// Wiring (per §3.5 / §5):
//   wiper = +3.3 V
//   each position contact -> diode -> code line (HIGH on selected pos)
//   each code line -> external 1 MΩ pull-down to GND
//   each code line -> one MCU GPIO (INPUT, high-Z; no internal pull-up)
//
// Behaviour:
//   - Pure polling loop, no sleep, no IRQ. Use rs1010_wiring_test for that.
//   - Debounces the inter-detent float glitch (all lines briefly read 0):
//     only accepts a code after two consecutive reads, separated by
//     SETTLE_MS, agree (per §7).
//   - Prints on every change and at PRINT_INTERVAL_MS heartbeat.

#define BAUD_RATE 9600

const uint8_t CODE_PINS[] = {10, 11, 12};   // b0, b1, b2
const uint8_t N_BITS = sizeof(CODE_PINS);

const unsigned long PRINT_INTERVAL_MS = 5000;
const uint8_t SETTLE_MS = 10;
const uint8_t MAX_SETTLE_TRIES = 20;

uint8_t lastCode = 0xFF;
unsigned long lastPrint = 0;

void setup() {
    Serial.begin(BAUD_RATE);

    for (uint8_t i = 0; i < N_BITS; i++) {
        pinMode(CODE_PINS[i], INPUT);
    }

    Serial.println("RS1010 polling test ready.");
    Serial.print("Code-line pins (b0..b");
    Serial.print(N_BITS - 1);
    Serial.print("): ");
    for (uint8_t i = 0; i < N_BITS; i++) {
        Serial.print("D");
        Serial.print(CODE_PINS[i]);
        if (i < N_BITS - 1) Serial.print(", ");
    }
    Serial.println();
}

void loop() {
    uint8_t code = readDebouncedCode();

    unsigned long now = millis();
    bool changed = (code != lastCode);
    bool tick = (now - lastPrint >= PRINT_INTERVAL_MS);

    if (changed || tick) {
        printReading(code, changed);
        lastCode = code;
        lastPrint = now;
    }
}

uint8_t readCodeOnce() {
    uint8_t code = 0;
    for (uint8_t i = 0; i < N_BITS; i++) {
        if (digitalRead(CODE_PINS[i]) == HIGH) {
            code |= (1 << i);
        }
    }
    return code;
}

// Two consecutive agreeing reads, separated by SETTLE_MS, defeat both the
// float-time all-zero glitch and any mid-rotation transient.
uint8_t readDebouncedCode() {
    uint8_t prev = readCodeOnce();
    for (uint8_t i = 0; i < MAX_SETTLE_TRIES; i++) {
        delay(SETTLE_MS);
        uint8_t cur = readCodeOnce();
        if (cur == prev) return cur;
        prev = cur;
    }
    return prev;
}

void printReading(uint8_t code, bool changed) {
    Serial.print(changed ? "* " : "  ");
    for (int i = N_BITS - 1; i >= 0; i--) {
        Serial.print("D");
        Serial.print(CODE_PINS[i]);
        Serial.print("=");
        Serial.print((code >> i) & 1);
        Serial.print(" ");
    }
    Serial.print(" bits=");
    for (int i = N_BITS - 1; i >= 0; i--) {
        Serial.print((code >> i) & 1);
    }
    Serial.print("  code=");
    Serial.println(code);
}
