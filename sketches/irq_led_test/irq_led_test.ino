// IRQ → LED test for ATmega328PB
//
// Monitors a GPIO pin via PCINT (Pin Change Interrupt).
// On any edge (rising or falling), lights the built-in LED for 3 seconds
// and logs events to the serial port at 9600 baud.
//
// Wiring: connect a pushbutton (or signal source) between TRIGGER_PIN and GND.
//         The pin is pulled up internally, so the idle state is HIGH and a press
//         pulls it LOW — both edges trigger the interrupt.
//
// Built-in LED: Arduino pin 13 (PB5) on standard 328PB boards.

#include <avr/io.h>
#include <avr/interrupt.h>

// ── configuration ────────────────────────────────────────────────────────────

// Any digital pin that belongs to a PCINT group works.
// Pin 2 = PD2 → PCINT18 → PCMSK2 / PCIE2
#define TRIGGER_PIN  2

#define LED_PIN      LED_BUILTIN   // pin 13

#define LED_ON_MS    3000UL        // ms to keep the LED on after an edge

#define BAUD_RATE    57600

// ── state shared with ISR ─────────────────────────────────────────────────────

volatile bool edge_detected = false;
volatile uint8_t pin_state  = 0;    // pin level sampled inside the ISR

// ── PCINT ISR ─────────────────────────────────────────────────────────────────

// PCINT2 vector covers PCINT16–PCINT23 (port D, Arduino pins 0–7).
// Adjust the vector name if you move TRIGGER_PIN to another port:
//   Port B (pins 8–13) → PCINT0_vect
//   Port C (pins A0–A5) → PCINT1_vect
//   Port D (pins 0–7)  → PCINT2_vect
ISR(PCINT2_vect)
{
    pin_state     = (PIND >> PD2) & 1;   // sample the pin immediately
    edge_detected = true;
}

// ── setup ────────────────────────────────────────────────────────────────────

void setup()
{
    Serial.begin(BAUD_RATE);
    Serial.println(F("[boot] IRQ-LED test started"));
    Serial.print(F("[boot] trigger=D"));
    Serial.print(TRIGGER_PIN);
    Serial.print(F("  led=D"));
    Serial.print(LED_PIN);
    Serial.print(F("  led_on_ms="));
    Serial.println(LED_ON_MS);

    pinMode(TRIGGER_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Enable PCINT for the trigger pin.
    // TRIGGER_PIN 2 = PD2 = PCINT18; bit position within PCMSK2 is (18 - 16) = 2.
    PCMSK2 |= _BV(PCINT18);   // unmask this specific pin
    PCICR  |= _BV(PCIE2);     // enable the PCINT2 group (port D)

    sei();
    Serial.println(F("[boot] PCINT armed, waiting for edges..."));
}

// ── main loop ─────────────────────────────────────────────────────────────────

void loop()
{
    if (edge_detected) {
        uint8_t state = pin_state;
        edge_detected = false;

        unsigned long t = millis();
        Serial.print(F("["));
        Serial.print(t);
        Serial.print(F(" ms] edge detected → pin "));
        Serial.println(state ? F("HIGH (rising)") : F("LOW (falling)"));

        Serial.println(F("[led] ON"));
        digitalWrite(LED_PIN, HIGH);
        delay(LED_ON_MS);
        digitalWrite(LED_PIN, LOW);
        Serial.println(F("[led] OFF"));
    }
}
