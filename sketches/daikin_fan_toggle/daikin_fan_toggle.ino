// Daikin fan-speed toggle — ATmega328PB at 4 MHz effective (CKDIV8 fuse active)
//
// Sends a real Daikin frame every 30 s, alternating fan speed between
// DAIKIN_FAN_1 (min) and DAIKIN_FAN_5 (max).  Power=ON, Mode=Cool, Temp=22°C.
//
// CKDIV8 note: IDE compiles for F_CPU=8000000 but chip runs at 4 MHz.
// delayMicroseconds(N) executes as ~2 N µs.  Every timing constant passed to
// mark()/space() is therefore halved before the call.
//
// Serial output at 2400 baud (sketch requests 4800; CKDIV8 halves the baud rate).
//
// Wiring: IR LED circuit on D3 (OC2B), same as ir_modulation_test.

#include <avr/io.h>

// Pull in the portable frame builder (path relative to sketch dir)
#include "../../firmware/daikin_frame.h"
#include "../../firmware/daikin_frame.cpp"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
#define IR_PIN 3   // OC2B — Timer2 compare output B

// ---------------------------------------------------------------------------
// Daikin protocol timing constants (µs), from IRremoteESP8266 ir_Daikin.h
// ---------------------------------------------------------------------------
#define DAIKIN_HDR_MARK    3650
#define DAIKIN_HDR_SPACE   1623
#define DAIKIN_BIT_MARK     428
#define DAIKIN_ONE_SPACE   1280
#define DAIKIN_ZERO_SPACE   428
#define DAIKIN_GAP        29000   // inter-section gap (space after last bit mark)

// ---------------------------------------------------------------------------
// Timer2 — 38 kHz carrier on OC2B (D3)
//
// F_CPU effective = 4 MHz (CKDIV8 fuse), prescaler /1.
// OCR2A = 52 → toggle period = 2 * (52+1) / 4e6 ≈ 26.5 µs → ~37.7 kHz.
// (The ir_modulation_test sketch used OCR2A not set explicitly — CTC mode with
//  HALF_PERIOD_US loops.  Here we use the hardware toggle so delayMicroseconds
//  controls only the burst duration, not the frequency.)
// ---------------------------------------------------------------------------
static void timer2_38khz_start() {
    // CTC mode, toggle OC2B on compare match, prescaler /1
    TCCR2A = (1 << COM2B0) | (1 << WGM21);
    TCCR2B = (1 << CS20);
    OCR2A  = 52;   // ~37.7 kHz at 4 MHz effective
    TCNT2  = 0;
    pinMode(IR_PIN, OUTPUT);
}

static void timer2_stop() {
    TCCR2A &= ~(1 << COM2B0);   // disconnect OC2B
    TCCR2B  = 0;                // stop clock
    digitalWrite(IR_PIN, LOW);
}

// ---------------------------------------------------------------------------
// mark / space
//
// All durations are in µs (protocol values).  We pass us/2 to
// delayMicroseconds() because CKDIV8 makes it run 2× slower.
// ---------------------------------------------------------------------------
static void ir_mark(uint16_t us) {
    TCCR2A |= (1 << COM2B0);    // enable OC2B toggle → carrier on
    delayMicroseconds(us / 2);
}

static void ir_space(uint16_t us) {
    TCCR2A &= ~(1 << COM2B0);   // disconnect OC2B → pin stays LOW
    digitalWrite(IR_PIN, LOW);
    delayMicroseconds(us / 2);
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
static void send_section(const uint8_t *data, uint8_t len) {
    ir_mark(DAIKIN_HDR_MARK);
    ir_space(DAIKIN_HDR_SPACE);
    for (uint8_t i = 0; i < len; i++) send_byte(data[i]);
    ir_mark(DAIKIN_BIT_MARK);
    ir_space(DAIKIN_GAP);
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
    ir_space(DAIKIN_GAP);

    send_section(frame,      8);   // section 1
    send_section(frame + 8,  8);   // section 2
    send_section(frame + 16, 19);  // section 3
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(4800);   // actual baud = 2400 (CKDIV8)
    timer2_38khz_start();
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

    send_daikin(frame);

    delay(30000);
}
