// daikin_serial — ATmega328P at 8 MHz
//
// Line-command serial interface to the Daikin IR transmitter.
// Used by tools/daikin_tui/ (Python Textual app) over USB serial at 115200 baud.
//
// Protocol (see 11_serial_remote_app.md §3):
//
//   Host → device:
//     PING
//     SEND fan=<0|1|2|3|4|5|quiet|auto> mode=<fan|cool|heat|dry|auto>
//          temp=<14..30> swing=<on|off>
//
//   Device → host:
//     READY daikin-serial/1.0    (boot)
//     PONG                       (answer to PING)
//     SENT <70 hex chars>        (35-byte frame, successfully transmitted)
//     ERR <reason>               (parse failure or out-of-range value)
//
// IR path: Timer2 38 kHz carrier on D3 (OC2B), same as daikin_fan_toggle.
// Clock:   CLKPR cleared so chip runs at nominal 8 MHz regardless of CKDIV8
//          fuse state (same idiom as ir_rx_dump).

#include <avr/io.h>
#include <string.h>
#include <stdlib.h>

#include "daikin_frame.h"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
#define IR_PIN 3   // OC2B — Timer2 compare output B

// ---------------------------------------------------------------------------
// Protocol timing constants (µs) — identical to daikin_fan_toggle
// ---------------------------------------------------------------------------
#define DAIKIN_HDR_MARK    3500
#define DAIKIN_HDR_SPACE   1700
#define DAIKIN_BIT_MARK     428
#define DAIKIN_ONE_SPACE   1280
#define DAIKIN_ZERO_SPACE   428
#define DAIKIN_LEADER_GAP 25000
#define DAIKIN_SECTION_GAP 35000

// ---------------------------------------------------------------------------
// Timer2 — 38 kHz carrier on OC2B (D3)
// ---------------------------------------------------------------------------
static void timer2_38khz_start() {
    TCCR2A = (1 << COM2B0) | (1 << WGM21);
    TCCR2B = (1 << CS20);
    OCR2A  = 104;
    TCNT2  = 0;
    pinMode(IR_PIN, OUTPUT);
}

// ---------------------------------------------------------------------------
// mark / space
// ---------------------------------------------------------------------------
static void ir_mark(uint16_t us) {
    TCCR2A |= (1 << COM2B0);
    delayMicroseconds(us);
}

static void ir_space(uint16_t us) {
    TCCR2A &= ~(1 << COM2B0);
    digitalWrite(IR_PIN, LOW);
    delayMicroseconds(us);
}

static void ir_space_long(uint32_t us) {
    TCCR2A &= ~(1 << COM2B0);
    digitalWrite(IR_PIN, LOW);
    while (us >= 16000) { delay(16); us -= 16000; }
    if (us) delayMicroseconds((uint16_t)us);
}

// ---------------------------------------------------------------------------
// send_byte / send_section / send_daikin — verbatim from daikin_fan_toggle
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

static void send_section(const uint8_t *data, uint8_t len, uint32_t gap) {
    ir_mark(DAIKIN_HDR_MARK);
    ir_space(DAIKIN_HDR_SPACE);
    for (uint8_t i = 0; i < len; i++) send_byte(data[i]);
    ir_mark(DAIKIN_BIT_MARK);
    ir_space_long(gap);
}

static void send_daikin(const uint8_t frame[DAIKIN_FRAME_LEN]) {
    for (uint8_t i = 0; i < 5; i++) {
        ir_mark(DAIKIN_BIT_MARK);
        ir_space(DAIKIN_ZERO_SPACE);
    }
    ir_mark(DAIKIN_BIT_MARK);
    ir_space_long(DAIKIN_LEADER_GAP);

    send_section(frame,      8,  DAIKIN_SECTION_GAP);
    send_section(frame + 8,  8,  DAIKIN_SECTION_GAP);
    send_section(frame + 16, 19, DAIKIN_SECTION_GAP);
}

// ---------------------------------------------------------------------------
// parse_and_send — parse a SEND line, transmit, reply SENT or ERR
//
// Expected format: SEND fan=<v> mode=<v> temp=<v> swing=<v>   (order-free)
// ---------------------------------------------------------------------------
static void parse_and_send(char *line) {
    ACState st;
    memset(&st, 0, sizeof(st));

    bool got_fan = false, got_mode = false, got_temp = false, got_swing = false;

    // Skip leading "SEND" token then tokenise by spaces
    char *tok = strtok(line, " ");  // "SEND"
    while ((tok = strtok(NULL, " ")) != NULL) {
        char *eq = strchr(tok, '=');
        if (!eq) { Serial.println("ERR malformed token (no =)"); return; }
        *eq = '\0';
        char *key = tok;
        char *val = eq + 1;

        if (strcmp(key, "fan") == 0) {
            if (strcmp(val, "0") == 0) {
                st.power = false;
                st.fan   = DAIKIN_FAN_AUTO;  // fan value doesn't matter when power=off
            } else if (strcmp(val, "auto")  == 0) { st.power = true; st.fan = DAIKIN_FAN_AUTO;  }
            else if (strcmp(val, "quiet") == 0) { st.power = true; st.fan = DAIKIN_FAN_QUIET; }
            else {
                int n = atoi(val);
                if (n < 1 || n > 5) { Serial.print("ERR fan "); Serial.print(val); Serial.println(" out of range 0..5,auto,quiet"); return; }
                st.power = true;
                st.fan   = (uint8_t)(n + 2);  // wire value: user 1..5 → 3..7
            }
            got_fan = true;
        } else if (strcmp(key, "mode") == 0) {
            if      (strcmp(val, "fan")  == 0) st.mode = DAIKIN_MODE_FAN;
            else if (strcmp(val, "cool") == 0) st.mode = DAIKIN_MODE_COOL;
            else if (strcmp(val, "heat") == 0) st.mode = DAIKIN_MODE_HEAT;
            else if (strcmp(val, "dry")  == 0) st.mode = DAIKIN_MODE_DRY;
            else if (strcmp(val, "auto") == 0) st.mode = DAIKIN_MODE_AUTO;
            else { Serial.print("ERR mode "); Serial.print(val); Serial.println(" unknown"); return; }
            got_mode = true;
        } else if (strcmp(key, "temp") == 0) {
            int t = atoi(val);
            if (t < 14 || t > 30) { Serial.print("ERR temp "); Serial.print(val); Serial.println(" out of range 14..30"); return; }
            st.temp = (uint8_t)t;
            got_temp = true;
        } else if (strcmp(key, "swing") == 0) {
            if      (strcmp(val, "on")  == 0) st.swing_v = true;
            else if (strcmp(val, "off") == 0) st.swing_v = false;
            else { Serial.print("ERR swing "); Serial.print(val); Serial.println(" must be on|off"); return; }
            got_swing = true;
        }
        // unknown keys are silently ignored (forward-compatibility)
    }

    if (!got_fan)   { Serial.println("ERR missing field: fan");   return; }
    if (!got_mode)  { Serial.println("ERR missing field: mode");  return; }
    if (!got_temp)  { Serial.println("ERR missing field: temp");  return; }
    if (!got_swing) { Serial.println("ERR missing field: swing"); return; }

    uint8_t frame[DAIKIN_FRAME_LEN];
    daikin_build_frame(&st, frame);

    digitalWrite(LED_BUILTIN, HIGH);
    send_daikin(frame);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.print("SENT ");
    for (uint8_t i = 0; i < DAIKIN_FRAME_LEN; i++) {
        if (frame[i] < 0x10) Serial.print('0');
        Serial.print(frame[i], HEX);
    }
    Serial.println();
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    // Clear CKDIV8 so the chip runs at nominal 8 MHz regardless of fuse state
    // (same idiom as ir_rx_dump).
    CLKPR = 0x80;
    CLKPR = 0x00;

    Serial.begin(115200);
    timer2_38khz_start();
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println("READY daikin-serial/1.0");
}

// Line buffer — longest realistic line: "SEND fan=quiet mode=auto temp=30 swing=on" = 43 chars
#define LINE_BUF_LEN 80

void loop() {
    static char buf[LINE_BUF_LEN];
    static uint8_t pos = 0;

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;  // ignore CR in CRLF line endings
        if (c == '\n') {
            buf[pos] = '\0';
            pos = 0;

            // Uppercase only the first word (command) for case-insensitive dispatch;
            // leave key=value pairs as-is so strcmp against lowercase literals works.
            for (char *p = buf; *p && *p != ' '; p++) {
                if (*p >= 'a' && *p <= 'z') *p -= 32;
            }

            if (strcmp(buf, "PING") == 0) {
                Serial.println("PONG");
            } else if (strncmp(buf, "SEND ", 5) == 0) {
                parse_and_send(buf);
            } else if (buf[0] != '\0') {
                Serial.print("ERR unknown command: ");
                Serial.println(buf);
            }
        } else {
            if (pos < LINE_BUF_LEN - 1) buf[pos++] = c;
            // silently drop chars beyond buffer; the line will likely fail to parse
        }
    }
}
