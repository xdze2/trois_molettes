#include "daikin_frame.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Frame layout (35 bytes = 3 sections)
//
//  Section 1  [0..7]   — fixed preamble, byte 7 = checksum
//  Section 2  [8..15]  — fixed preamble + clock (unused), byte 15 = checksum
//  Section 3  [16..34] — AC state, byte 34 = checksum
//
// Reference: IRDaikinESP in IRremoteESP8266/src/ir_Daikin.{h,cpp}
// ---------------------------------------------------------------------------

// Sum of bytes [start .. start+len-1], truncated to 8 bits.
static uint8_t sum_bytes(const uint8_t *buf, uint8_t len) {
    uint8_t s = 0;
    for (uint8_t i = 0; i < len; i++) s += buf[i];
    return s;
}

void daikin_build_frame(const ACState *st, uint8_t frame[DAIKIN_FRAME_LEN]) {

    // --- Zero the whole frame first ---
    memset(frame, 0, DAIKIN_FRAME_LEN);

    // -----------------------------------------------------------------------
    // Section 1 [0..7] — fixed preamble
    // -----------------------------------------------------------------------
    frame[0] = 0x11;
    frame[1] = 0xDA;
    frame[2] = 0x27;
    // frame[3] = 0x00  (already zero)
    frame[4] = 0xC5;
    // frame[5..6] = 0x00
    frame[7] = sum_bytes(frame, 7);   // checksum

    // -----------------------------------------------------------------------
    // Section 2 [8..15] — fixed preamble + clock field (left at 0 = unused)
    // -----------------------------------------------------------------------
    frame[8]  = 0x11;
    frame[9]  = 0xDA;
    frame[10] = 0x27;
    // frame[11] = 0x00
    frame[12] = 0x42;
    // frame[13..14] = clock bits, left 0x00 (clock unused)
    frame[15] = sum_bytes(frame + 8, 7);  // checksum

    // -----------------------------------------------------------------------
    // Section 3 [16..34] — AC state
    // -----------------------------------------------------------------------
    frame[16] = 0x11;
    frame[17] = 0xDA;
    frame[18] = 0x27;
    // frame[19..20] = 0x00

    // Byte 21: 0x49 is the reset value with timers off.
    // 0x49 = 0b0100_1001: bit0=Power, bit3=always1, bits6:4=Mode, others=timer/reserved.
    // We start from 0x49 (timers off, reserved bits as per reference) then overlay power+mode.
    frame[21] = 0x49;
    frame[21] &= ~(1 << 0);                   // clear power bit
    frame[21] &= ~(0x07 << 4);                // clear mode bits
    if (st->power) frame[21] |= (1 << 0);
    frame[21] |= (st->mode & 0x07) << 4;

    // Byte 22: temperature. Protocol encodes as (celsius * 2), so 16°C → 0x20, 26°C → 0x34.
    // Reference: setTemp() does degrees * 2.0. Reset default is 0x1E = 30 → 15°C; we always
    // overwrite from the knob so the reset default doesn't matter.
    uint8_t temp = st->temp;
    if (temp < 10) temp = 10;
    if (temp > 32) temp = 32;
    frame[22] = temp * 2;

    // Byte 23: 0x00 (reserved / not used by this project)

    // Byte 24: Fan (high nibble) | SwingV (low nibble)
    // Fan wire values: auto=0xA, quiet=0xB, speed1..5 = 3..7 (= user_speed + 2)
    frame[24]  = (st->fan & 0x0F) << 4;
    frame[24] |= st->swing_v ? 0x0F : 0x00;

    // Byte 25: SwingH (low nibble) — not exposed as a control; leave off.
    frame[25] = 0x00;

    // Bytes 26..28: OnTime + OffTime fields packed across 3 bytes.
    // kDaikinUnusedTime = 0x600 for both; this produces the fixed values
    // raw[27]=0x06, raw[28]=0x60 seen in every real capture with timers off.
    frame[27] = 0x06;
    frame[28] = 0x60;

    // Byte 29: Powerful(bit0) | Quiet(bit5)
    // Powerful and Quiet are mutually exclusive; we trust ACState is consistent.
    if (st->powerful) frame[29] |= (1 << 0);

    // Byte 30: reserved, left 0x00.
    // Byte 31: 0xC0 — two reserved bits always set per reference stateReset().
    frame[31] = 0xC0;

    // Byte 32: Econo(bit2). WeeklyTimer(bit7) and other reserved bits are 0x00 here
    // (reference stateReset sets 0xC0 at [31] and 0x00 at [32]).
    if (st->econo) frame[32] |= (1 << 2);

    // Byte 33: Mold bit — not exposed; left 0x00.

    // Byte 34: checksum of section 3
    frame[34] = sum_bytes(frame + 16, 18);
}
