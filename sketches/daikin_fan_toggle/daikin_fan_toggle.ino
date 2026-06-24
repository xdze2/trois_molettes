// Daikin fan-speed toggle — ATmega328PB at 8 MHz
//
// Sends a real Daikin frame every 30 s, alternating fan speed between
// DAIKIN_FAN_1 (min) and DAIKIN_FAN_5 (max).  Power=ON, Mode=Cool, Temp=22°C.
//
// Clock note: measured 66.7 kHz carrier with OCR2A=58 confirms the chip runs at
// the nominal 8 MHz (CKDIV8 fuse is NOT active), so delayMicroseconds() and
// Serial baud rate work as specified — no scaling needed.
//
// Serial output at 4800 baud.
//
// Wiring: IR LED circuit on D3 (OC2B), same as ir_modulation_test.

#include <avr/io.h>

// Portable frame builder.  The .h / .cpp files in this sketch dir are symlinks
// to ../../firmware/ — Arduino IDE only compiles files inside the sketch folder.
#include "daikin_frame.h"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
#define IR_PIN 3   // OC2B — Timer2 compare output B

// ---------------------------------------------------------------------------
// Daikin protocol timing constants (µs).
//
// Bit-level values follow IRremoteESP8266 ir_Daikin.h.  The header and gap
// values are taken from the REAL ARC466A33 capture (howtos/data/dump_fan_loop.txt,
// decoded in howto 07), which diverges from the library's nominal numbers:
//
//   - Header mark/space measured ~3492 / ~1712 µs (library says 3650 / 1623).
//   - The library uses a single 29 ms gap everywhere; the real remote uses
//     ~25 ms after the preamble and ~35 ms between sections.  Using one 29 ms
//     value for both is wrong on this remote.
// ---------------------------------------------------------------------------
#define DAIKIN_HDR_MARK    3500
#define DAIKIN_HDR_SPACE   1700
#define DAIKIN_BIT_MARK     428
#define DAIKIN_ONE_SPACE   1280
#define DAIKIN_ZERO_SPACE   428
#define DAIKIN_LEADER_GAP 25000   // gap after the 0b00000 preamble (real: ~25 ms)
#define DAIKIN_SECTION_GAP 35000  // gap between data sections (real: ~35 ms)

// ---------------------------------------------------------------------------
// Timer2 — 38 kHz carrier on OC2B (D3)
//
// F_CPU = 8 MHz, prescaler /1.
// OCR2A = 104 → toggle period = 2 * (104+1) / 8e6 = 26.25 µs → ~38.1 kHz.
// ---------------------------------------------------------------------------
static void timer2_38khz_start() {
    // CTC mode, toggle OC2B on compare match, prescaler /1
    TCCR2A = (1 << COM2B0) | (1 << WGM21);
    TCCR2B = (1 << CS20);
    OCR2A  = 104;  // ~38.1 kHz at F_CPU = 8 MHz
    TCNT2  = 0;
    pinMode(IR_PIN, OUTPUT);
}

static void timer2_stop() {
    TCCR2A &= ~(1 << COM2B0);   // disconnect OC2B
    TCCR2B  = 0;                // stop clock
    digitalWrite(IR_PIN, LOW);
}

// ---------------------------------------------------------------------------
// mark / space — all durations are in µs (protocol values).
// ---------------------------------------------------------------------------
static void ir_mark(uint16_t us) {
    TCCR2A |= (1 << COM2B0);    // enable OC2B toggle → carrier on
    delayMicroseconds(us);
}

static void ir_space(uint16_t us) {
    TCCR2A &= ~(1 << COM2B0);   // disconnect OC2B → pin stays LOW
    digitalWrite(IR_PIN, LOW);
    delayMicroseconds(us);
}

// delayMicroseconds() is only accurate up to ~16383 µs.  The Daikin gaps
// (25 ms leader, 35 ms inter-section) exceed that and silently collapse to
// near-zero, fusing all sections into one ~300 ms blob on the scope.
static void ir_space_long(uint32_t us) {
    TCCR2A &= ~(1 << COM2B0);
    digitalWrite(IR_PIN, LOW);
    while (us >= 16000) { delay(16); us -= 16000; }
    if (us) delayMicroseconds((uint16_t)us);
}

// ---------------------------------------------------------------------------
// sendByte — LSB first, as per Daikin protocol
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

// ---------------------------------------------------------------------------
// sendSection — header + data bytes + trailing bit-mark + gap
// ---------------------------------------------------------------------------
static void send_section(const uint8_t *data, uint8_t len, uint32_t gap) {
    ir_mark(DAIKIN_HDR_MARK);
    ir_space(DAIKIN_HDR_SPACE);
    for (uint8_t i = 0; i < len; i++) send_byte(data[i]);
    ir_mark(DAIKIN_BIT_MARK);
    ir_space_long(gap);
}

// ---------------------------------------------------------------------------
// sendDaikin — matches the 3-section structure in IRremoteESP8266
//
// Section 1: bytes  0.. 7  (8 bytes)
// Section 2: bytes  8..15  (8 bytes)
// Section 3: bytes 16..34  (19 bytes)
//
// A 5-bit "00000" preamble precedes section 1, terminated by a gap.
// ---------------------------------------------------------------------------
static void send_daikin(const uint8_t frame[DAIKIN_FRAME_LEN]) {
    // 5-bit preamble (all zeros), no section header
    for (uint8_t i = 0; i < 5; i++) {
        ir_mark(DAIKIN_BIT_MARK);
        ir_space(DAIKIN_ZERO_SPACE);
    }
    ir_mark(DAIKIN_BIT_MARK);
    ir_space_long(DAIKIN_LEADER_GAP);

    send_section(frame,      8,  DAIKIN_SECTION_GAP);   // section 1
    send_section(frame + 8,  8,  DAIKIN_SECTION_GAP);   // section 2
    send_section(frame + 16, 19, DAIKIN_SECTION_GAP);   // section 3
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(4800);
    timer2_38khz_start();
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("Daikin fan toggle — MIN/MAX every 30 s");
}

void loop() {
    static bool fan_max = false;
    fan_max = !fan_max;

    ACState st;
    st.power    = true;
    st.mode     = DAIKIN_MODE_COOL;
    st.temp     = 22;
    st.fan      = fan_max ? DAIKIN_FAN_5 : DAIKIN_FAN_1;
    st.swing_v  = false;
    st.powerful = false;
    st.econo    = false;

    uint8_t frame[DAIKIN_FRAME_LEN];
    daikin_build_frame(&st, frame);

    Serial.print("Sending fan=");
    Serial.println(fan_max ? "MAX (5)" : "MIN (1)");

    digitalWrite(LED_BUILTIN, HIGH);
    send_daikin(frame);          // ~120 ms of IR transmission
    delay(400);                  // hold LED on long enough to see
    digitalWrite(LED_BUILTIN, LOW);

    delay(4000);
}
