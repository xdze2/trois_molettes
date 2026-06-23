// ir_rx_dump — listen to a TSOP IR receiver and print raw pulse/space timings
//
// Wiring: TSOP38238 OUT pin → D2 (any digital pin works, change IR_RX_PIN).
//         TSOP is active-low: LOW = carrier burst (mark), HIGH = silence (space).
//
// Output at 9600 baud, one line per pulse or space:
//   M 3650     ← mark  (IR burst)  duration in µs
//   S 1623     ← space (silence)   duration in µs
//   ---        ← gap > GAP_TIMEOUT printed as frame separator
//
// Capture on macOS:
//   screen /dev/cu.usbserial-XXXX 9600   (Ctrl-A Ctrl-\ to quit)
//   OR to file:
//   stty -f /dev/cu.usbserial-XXXX 9600 raw && cat /dev/cu.usbserial-XXXX > dump.txt

#define IR_RX_PIN    2      // TSOP OUT — active LOW
#define GAP_TIMEOUT  65000  // µs — anything longer ends the frame
#define BAUD         9600

void setup() {
    Serial.begin(BAUD);
    pinMode(IR_RX_PIN, INPUT);
    Serial.println("# ir_rx_dump ready — point remote at TSOP and press a button");
}

void loop() {
    // Wait for the line to go LOW (start of a mark), timeout = GAP_TIMEOUT
    uint32_t dur = pulseIn(IR_RX_PIN, LOW, GAP_TIMEOUT);

    if (dur == 0) {
        // Nothing received — idle
        return;
    }

    // Print the mark
    Serial.print("M ");
    Serial.println(dur);

    // Now read alternating spaces and marks until we time out
    while (true) {
        uint32_t space = pulseIn(IR_RX_PIN, HIGH, GAP_TIMEOUT);
        if (space == 0) {
            Serial.println("---");
            break;
        }
        Serial.print("S ");
        Serial.println(space);

        uint32_t mark = pulseIn(IR_RX_PIN, LOW, GAP_TIMEOUT);
        if (mark == 0) {
            Serial.println("---");
            break;
        }
        Serial.print("M ");
        Serial.println(mark);
    }
}
