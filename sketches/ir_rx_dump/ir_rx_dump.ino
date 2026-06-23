// ir_rx_dump — listen to a TSOP IR receiver and print raw pulse/space timings
//
// Wiring: TSOP38238 OUT pin → D2 (any digital pin works, change IR_RX_PIN).
//         TSOP is active-low: LOW = carrier burst (mark), HIGH = silence (space).
//
// Output at 115200 baud:
//
//   LEADER  2 pulses
//   gap     25432 us
//   SECTION 1
//   M 3492     <- section header mark (~3.5 ms)
//   S 1266     <- section header space (~1.7 ms)
//   M 454      <- data bit mark  (~450 us, always)
//   S 402      <- data bit space (<700 us -> bit 0, >700 us -> bit 1)
//   ...
//   --- 34800  <- inter-section gap (~35 ms in Daikin)
//   SECTION 2
//   ...
//   ===        <- end-of-frame gap (>60 ms)
//
// Capture on macOS:
//   uv run sketches/ir_rx_dump/capture.py /dev/cu.usbserial-XXXX
//   uv run sketches/ir_rx_dump/capture.py /dev/cu.usbserial-XXXX -o dump.txt

#include <avr/io.h>

#define IR_RX_PIN    2
#define LEADER_MAX   1000    // us: marks shorter than this belong to the leader
#define SECTION_GAP  10000   // us: spaces longer than this are inter-section gaps
#define FRAME_GAP    60000   // us: spaces longer than this end the frame
#define BAUD         115200

void setup() {
    CLKPR = 0x80;
    CLKPR = 0x00;  // full 8 MHz, clear any CKDIV8 left by bootloader
    Serial.begin(BAUD);
    pinMode(IR_RX_PIN, INPUT);
    Serial.println("# ir_rx_dump ready");
}

// Measure the duration of the next LOW pulse (mark), capped at FRAME_GAP.
// pulseIn(LOW) waits for a HIGH->LOW transition then times the LOW pulse.
// This is accurate because it uses the hardware timer via the Arduino core.
static uint32_t readMark() {
    uint32_t d = pulseIn(IR_RX_PIN, LOW, FRAME_GAP);
    return (d == 0) ? FRAME_GAP : d;
}

// Measure the space (HIGH period) that is already in progress when called.
// pulseIn(HIGH) would miss it because the pin is already HIGH.
// Instead: record micros() now (end of mark), then wait for the next LOW,
// record micros() again (start of next mark) — the delta is the space.
// The next mark duration is returned via the out-pointer so it can be reused.
static uint32_t readSpaceThenMark(uint32_t *next_mark) {
    uint32_t t0 = micros();                          // end of current mark
    *next_mark = pulseIn(IR_RX_PIN, LOW, FRAME_GAP); // waits for next mark, times it
    uint32_t t1 = micros();
    if (*next_mark == 0) {
        *next_mark = FRAME_GAP;
        return FRAME_GAP;
    }
    // t1 is now micros() *after* the LOW pulse ended, so subtract the mark duration
    uint32_t space = (t1 - t0) - *next_mark;
    return space;
}

void loop() {
    // ── Wait for and measure the first mark ──────────────────────────────────
    uint32_t dur = readMark();
    if (dur >= FRAME_GAP) return;

    // ── Leader: consume short mark/space pairs ───────────────────────────────
    uint8_t leader_count = 0;

    while (dur < LEADER_MAX) {
        leader_count++;
        uint32_t next_mark;
        uint32_t sp = readSpaceThenMark(&next_mark);
        dur = next_mark;
        if (sp >= SECTION_GAP) {
            Serial.print("LEADER  "); Serial.print(leader_count); Serial.println(" pulses");
            Serial.print("gap     "); Serial.print(sp); Serial.println(" us");
            leader_count = 0;  // already printed
            break;
        }
        // short space: still in leader, continue with dur = next_mark
    }
    if (leader_count > 0) {
        // leader ended because dur >= LEADER_MAX (no explicit long gap)
        Serial.print("LEADER  "); Serial.print(leader_count); Serial.println(" pulses");
    }

    // ── Sections ─────────────────────────────────────────────────────────────
    uint8_t section = 1;
    while (true) {
        Serial.print("SECTION "); Serial.println(section);
        Serial.print("M "); Serial.println(dur);   // section header mark

        uint32_t next_mark;
        uint32_t sp = readSpaceThenMark(&next_mark);
        Serial.print("S "); Serial.println(sp);    // section header space
        dur = next_mark;

        if (sp >= FRAME_GAP) { Serial.println("==="); return; }

        // ── Data bits ────────────────────────────────────────────────────────
        while (true) {
            // dur already holds the current bit mark (measured by readSpaceThenMark)
            sp = readSpaceThenMark(&next_mark);

            Serial.print("M "); Serial.println(dur);

            if (sp >= FRAME_GAP) { Serial.println("==="); return; }

            if (sp >= SECTION_GAP) {
                Serial.print("--- "); Serial.println(sp);
                section++;
                dur = next_mark;
                break;  // outer loop: print SECTION label
            }

            Serial.print("S "); Serial.println(sp);
            dur = next_mark;
        }
    }
}
