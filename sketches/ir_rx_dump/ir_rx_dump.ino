// ir_rx_dump — listen to a TSOP IR receiver and print raw pulse/space timings
//
// Strategy (see howtos/06_ir_rx_dump.md "Recommended fix"):
//   A pin-change interrupt (PCINT) timestamps EVERY edge on D2 into a ring
//   buffer. The ISR does the bare minimum — record micros() — so it never
//   misses a short ~400 us space. All the slow work (Serial.print, decode)
//   happens in loop() AFTER the frame is over, when the remote is silent and
//   there is zero timing pressure.
//
// Wiring: TSOP38238 OUT pin → D2.
//         TSOP is active-low: LOW = carrier burst (mark), HIGH = silence (space).
//
// Output at 115200 baud:
//
//   LEADER  6 pulses
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
#include <avr/interrupt.h>

#define IR_RX_PIN 2
#define LEADER_MAX 1000   // us: marks shorter than this belong to the leader
#define SECTION_GAP 10000 // us: spaces longer than this are inter-section gaps
#define FRAME_GAP 60000   // us: silence longer than this means the frame is done
#define BAUD 115200

#define BUF_LEN 768 // ring buffer: 1 uint16 delta per edge.
// One Daikin frame ≈ 320 edges, but a single button press emits frame+repeat
// (~700 edges total) before the inter-press silence. 768 × 2 B = 1536 B SRAM;
// leaves ~500 B for Serial buffers, stack, and globals on the 2 KB ATmega328P.

// ── Shared between the ISR and loop() ────────────────────────────────────────
// `volatile` tells the compiler these can change behind its back (inside the
// ISR), so it must re-read them from memory instead of caching in a register.
volatile uint16_t edges[BUF_LEN]; // µs duration of each interval (delta, not absolute)
volatile uint16_t edge_count = 0; // how many edges captured so far
volatile bool buf_overflow = false;
volatile uint32_t last_edge_time = 0; // absolute micros() of the last edge, for frame-end detection
volatile uint32_t isr_prev = 0;       // previous edge timestamp, reset between frames
volatile bool first_interval_is_mark = true; // edges[1] is a mark when the pin is HIGH at end of ISR2

// ── ISR: fires on every CHANGE (rising OR falling) of any pin in PCINT2 group ─
// D2 is PD2, which lives in the PCINT2 group (PCINT16..23 = PORTD).
// We store deltas (uint16_t) rather than absolute timestamps (uint32_t) so
// 512 entries fit in ~1 KB instead of 2 KB, leaving room for stack/heap.
// Each interval is at most ~35 ms = 35000 µs, well within uint16_t range.
// edges[0] is the time since the ISR was armed (discarded in decode).
ISR(PCINT2_vect)
{
    uint32_t now = micros();
    last_edge_time = now;
    if (edge_count < BUF_LEN)
    {
        uint16_t idx = edge_count++;
        edges[idx] = (uint16_t)(now - isr_prev);
        isr_prev = now;
        if (idx == 1) // first real interval: record whether it's a mark or space.
            // The ISR fires AFTER the edge, so PIND reflects the level during
            // the NEXT interval. If pin is HIGH now, edges[1] was a mark
            // (carrier just stopped); if LOW, edges[1] was a space.
            first_interval_is_mark = (PIND & (1 << PD2));
    }
    else
    {
        buf_overflow = true;
    }
}

void setup()
{
    CLKPR = 0x80;
    CLKPR = 0x00; // full 8 MHz, clear any CKDIV8 left by bootloader
    Serial.begin(BAUD);
    pinMode(IR_RX_PIN, INPUT);

    // Enable pin-change interrupt on D2 (PD2 / PCINT18).
    PCICR |= (1 << PCIE2);    // turn on the PCINT2 group (PORTD)
    PCIFR |= (1 << PCIF2);    // clear any stale pending flag (write-1-to-clear)
    PCMSK2 |= (1 << PCINT18); // ...but only watch PD2 within that group
    sei();                    // global interrupt enable

    Serial.println("# ir_rx_dump ready");
}

// Decode one captured frame from the `edges` buffer and print it.
//   n     = number of edges in this frame
//   first = the pin level (HIGH/LOW) at the moment of the FIRST recorded edge?
//
// Each consecutive pair of timestamps is one interval. We must know whether an
// interval is a MARK (carrier present, pin LOW) or a SPACE (silence, pin HIGH).
// Because the TSOP idles HIGH, the very first edge is HIGH→LOW: the interval
// AFTER it is a mark. So intervals alternate mark, space, mark, space, ...
static void decodeFrame(uint16_t n, bool is_buf_overflow, bool first_is_mark)
{
    // edges[] holds deltas: edges[0] is time-since-arm (junk), skip it.
    // edges[1] is the first real interval; whether it's a mark depends on which
    // edge triggered first (normally falling = mark, but not guaranteed).
    for (uint16_t i = 1; i < n; i++)
    {
        uint32_t duration = edges[i];
        bool is_mark = ((i % 2 == 1) == first_is_mark); // honour actual phase
        if (is_mark)
        {
            Serial.print("M ");
            Serial.println(duration);
        }
        else // space
        {
            if (duration >= FRAME_GAP)
            {
                Serial.println("===");
                break;
            }
            else if (duration >= SECTION_GAP)
            {
                Serial.print("--- ");
                Serial.println(duration);
            }
            else
            {
                Serial.print("S ");
                Serial.println(duration);
            }
        }
    }
    if (is_buf_overflow)
    {
        Serial.println("buf_overflow");
    }
}

void loop()
{
    // Wait until at least one edge has arrived AND the line has gone quiet for
    // longer than FRAME_GAP — that means the remote finished transmitting.
    if (edge_count == 0)
        return;

    // Snapshot edge_count and last_edge_time atomically. Both are multi-byte
    // volatiles written by the ISR; a non-atomic read can tear, so wrap in cli/sei.
    uint16_t count_now;
    uint32_t last_edge_snapshot;
    uint8_t sreg = SREG;
    cli();
    count_now = edge_count;
    last_edge_snapshot = last_edge_time;
    SREG = sreg;

    // Always wait for true silence (FRAME_GAP since last edge) before decoding,
    // even on buffer overflow. Re-arming the ISR mid-frame guarantees corrupt
    // continuation: decodeFrame() prints ~512 lines @ 115200 baud (~50 ms)
    // while the ISR is disabled, so many edges are lost. The next edge then
    // resyncs `first_interval_is_mark` to the wrong phase, producing M-M / S-S
    // adjacencies in the output. Better to truncate at BUF_LEN and wait out
    // the rest of the press; the ISR keeps updating `last_edge_time` so the
    // gap is still detected correctly.
    if ((micros() - last_edge_snapshot) < FRAME_GAP)
        return; // still receiving (possibly past buffer end) — come back later

    // Frame is done. Freeze the buffer: disable the ISR so nothing mutates
    // `edges`/`edge_count` while we decode and print (printing is slow).
    PCMSK2 &= ~(1 << PCINT18);

    decodeFrame(count_now, buf_overflow, first_interval_is_mark);
    Serial.println("==="); // inter-press silence ended the frame (not in buffer)

    // Reset for the next button press and re-arm the interrupt.
    edge_count = 0;
    buf_overflow = false;
    isr_prev = micros(); // anchor delta timing to now so edges[0] is not stale
    PCIFR |= (1 << PCIF2); // discard any edge that arrived while ISR was disabled
    PCMSK2 |= (1 << PCINT18);
}
