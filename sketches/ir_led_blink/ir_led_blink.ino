// Simple IR LED blink — toggle D3 every 1 s.
//
// No 38 kHz carrier: D3 is just driven HIGH then LOW. The IR LED itself emits
// invisibly, so swap in a red LED (with appropriate series resistor) to see it.
//
// Wiring matches howto 06: D3 -> R_base (2.2k) -> S9013 base, LED on collector.
// GPIO HIGH -> transistor saturates -> LED ON.
// GPIO LOW  -> transistor off       -> LED OFF.

#define LED_PIN 2
#define BAUD_RATE 4800

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(BAUD_RATE);
    Serial.println("ir_led_blink: D3 + onboard LED toggle every 1 s");
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("ON");
    delay(1000);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("OFF");
    delay(1000);
}
