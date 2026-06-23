// GPIO test for ATmega328PB
// Set specific pins HIGH or LOW and read back their state via Serial

#define BAUD_RATE 9600

// Pins to configure as outputs and drive HIGH
const uint8_t OUTPUT_PINS[] = {2, 3, 4, 5, 6, 7, 8, 9};
const uint8_t N_OUTPUTS = sizeof(OUTPUT_PINS);

// Pins to configure as inputs (with pull-up) and read
const uint8_t INPUT_PINS[] = {A0, A1, A2, A3};
const uint8_t N_INPUTS = sizeof(INPUT_PINS);

void setup() {
    Serial.begin(BAUD_RATE);

    for (uint8_t i = 0; i < N_OUTPUTS; i++) {
        pinMode(OUTPUT_PINS[i], OUTPUT);
        digitalWrite(OUTPUT_PINS[i], LOW);
    }

    for (uint8_t i = 0; i < N_INPUTS; i++) {
        pinMode(INPUT_PINS[i], INPUT_PULLUP);
    }

    Serial.println("ATmega328PB GPIO test ready.");
    Serial.println("Commands:");
    Serial.println("  h<pin>  set pin HIGH  (e.g. h3)");
    Serial.println("  l<pin>  set pin LOW   (e.g. l3)");
    Serial.println("  r<pin>  read pin      (e.g. rA0)");
    Serial.println("  s       status of all configured pins");
}

void loop() {
    if (Serial.available() == 0) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    char action = cmd[0];
    String pinStr = cmd.substring(1);

    if (action == 's') {
        printStatus();
        return;
    }

    int pin = parsePin(pinStr);
    if (pin < 0) {
        Serial.print("Unknown pin: ");
        Serial.println(pinStr);
        return;
    }

    if (action == 'h') {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        Serial.print("Pin ");
        Serial.print(pin);
        Serial.println(" -> HIGH");
    } else if (action == 'l') {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        Serial.print("Pin ");
        Serial.print(pin);
        Serial.println(" -> LOW");
    } else if (action == 'r') {
        int val = digitalRead(pin);
        Serial.print("Pin ");
        Serial.print(pin);
        Serial.print(" = ");
        Serial.println(val ? "HIGH" : "LOW");
    } else {
        Serial.println("Unknown command. Use h/l/r/s.");
    }
}

// Accept "A0".."A5" or plain integers
int parsePin(const String& s) {
    if (s.startsWith("A") || s.startsWith("a")) {
        int idx = s.substring(1).toInt();
        if (idx >= 0 && idx <= 5) return A0 + idx;
        return -1;
    }
    int n = s.toInt();
    if (n == 0 && s != "0") return -1;
    return n;
}

void printStatus() {
    Serial.println("--- Output pins ---");
    for (uint8_t i = 0; i < N_OUTPUTS; i++) {
        Serial.print("  D");
        Serial.print(OUTPUT_PINS[i]);
        Serial.print(": ");
        Serial.println(digitalRead(OUTPUT_PINS[i]) ? "HIGH" : "LOW");
    }
    Serial.println("--- Input pins ---");
    for (uint8_t i = 0; i < N_INPUTS; i++) {
        Serial.print("  A");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(digitalRead(INPUT_PINS[i]) ? "HIGH" : "LOW");
    }
}
