// RS1010 wiring + sleep/wake test (ATmega328P / 328PB)
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
//   - Debounces the inter-detent float glitch (all lines briefly read 0):
//     only accepts a code after two consecutive reads, separated by
//     SETTLE_MS, agree (per §7).
//   - Sleeps in SLEEP_MODE_PWR_DOWN between events; wakes on any edge of
//     any of the three code lines via PCINT (per §3.5 wake scheme).
//
// Pin requirement:
//   D10/D11/D12 on the 328P are PB2/PB3/PB4 -> PCINT2/3/4 -> PCI vector
//   PCINT0_vect. If you change CODE_PINS, update the PCINT group too.

#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/power.h>

#define BAUD_RATE 9600

const uint8_t CODE_PINS[] = {10, 11, 12};   // b0, b1, b2  (PB2, PB3, PB4)
const uint8_t N_BITS = sizeof(CODE_PINS);

const unsigned long PRINT_INTERVAL_MS = 5000;
const uint8_t SETTLE_MS = 10;       // debounce settle window
const uint8_t MAX_SETTLE_TRIES = 20; // ~100 ms hard cap

volatile bool wakeFlag = false;

uint8_t lastCode = 0xFF;
unsigned long lastPrint = 0;

void setup() {
    Serial.begin(BAUD_RATE);

    for (uint8_t i = 0; i < N_BITS; i++) {
        pinMode(CODE_PINS[i], INPUT);
    }

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // awake

    enablePCINT();

    Serial.println("RS1010 wiring + sleep/wake test ready.");
    Serial.print("Code-line pins (b0..b");
    Serial.print(N_BITS - 1);
    Serial.print("): ");
    for (uint8_t i = 0; i < N_BITS; i++) {
        Serial.print("D");
        Serial.print(CODE_PINS[i]);
        if (i < N_BITS - 1) Serial.print(", ");
    }
    Serial.println();
    Serial.flush();
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

    // Sleep until a pin-change IRQ on any code line.
    Serial.flush();
    sleepUntilWake();
}

// --- Reading -----------------------------------------------------------

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
    return prev; // give up, return last sample
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

// --- Sleep / wake ------------------------------------------------------

void enablePCINT() {
    // CODE_PINS = D10/D11/D12 = PB2/PB3/PB4 -> PCINT2/3/4, group PCIE0.
    cli();
    PCICR  |= _BV(PCIE0);
    PCMSK0 |= _BV(PCINT2) | _BV(PCINT3) | _BV(PCINT4);
    sei();
}

ISR(PCINT0_vect) {
    wakeFlag = true;
}

// Power-down sleep. Wakes on PCINT (any code-line edge).
// millis()/Timer0 are stopped in power-down, so there is no time-based
// wake here — only the GPIO IRQ. For a periodic heartbeat, add WDT.
void sleepUntilWake() {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli();
    wakeFlag = false;
    sleep_enable();
    sei();
    digitalWrite(LED_BUILTIN, LOW); // sleeping
    sleep_cpu();
    // Execution resumes here after PCINT0_vect.
    sleep_disable();
    digitalWrite(LED_BUILTIN, HIGH); // awake
}
