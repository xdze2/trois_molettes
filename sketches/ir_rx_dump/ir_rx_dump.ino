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

#define BUF_LEN 256 // ring buffer: 1 uint32 per edge (~200 needed/frame)

// ── Shared between the ISR and loop() ────────────────────────────────────────
// `volatile` tells the compiler these can change behind its back (inside the
// ISR), so it must re-read them from memory instead of caching in a register.
volatile uint32_t edges[BUF_LEN]; // micros() timestamp of each edge
volatile uint16_t edge_count = 0; // how many edges captured so far
volatile bool buf_overflow = false;

// ── ISR: fires on every CHANGE (rising OR falling) of any pin in PCINT2 group ─
// D2 is PD2, which lives in the PCINT2 group (PCINT16..23 = PORTD).
// We keep this as short as humanly possible: one micros() read, one store.
ISR(PCINT2_vect)
{
    if (edge_count < BUF_LEN)
    {
        edges[edge_count++] = micros();
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
static void decodeFrame(uint16_t n, bool is_buf_overflow)
{
    // We have n timestamps → n-1 intervals between them.
    // interval i = edges[i+1] - edges[i]
    // i even  → mark   (pin was LOW during this interval)
    // i odd   → space  (pin was HIGH during this interval)

    // ── YOUR EXERCISE STARTS HERE ──────────────────────────────────────────
    //
    // TODO 1: Loop over the intervals (i from 0 to n-2). Compute `dur` for each.
    for (uint16_t i = 0; i + 1 < n; i++)
    {
        uint32_t duration = edges[i + 1] - edges[i];
        // TODO 2: Classify each interval:
        //         - i even  → it's a mark  → print  "M <dur>"
        //         - i odd   → it's a space → decide what to print based on size:
        //             * dur >= FRAME_GAP   → print "==="   and stop
        //             * dur >= SECTION_GAP → print "--- <dur>"  (inter-section gap)
        //             * otherwise          → print "S <dur>"
        if (i % 2 == 0)
        {
            Serial.print("M ");
            Serial.println(duration);
        }
        else
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
    //
    // TODO 3 (stretch): track section boundaries and print "SECTION n"
    //         headers + the "LEADER / gap" lines like the example output.
    //         You can do this in a second pass once the basic M/S dump works.
    //
    // Hint: keep it simple first — just get clean alternating "M ..." / "S ..."
    //       lines printing. Add SECTION/LEADER labelling only once that works.
    //
    // (write your loop here)

    // ── YOUR EXERCISE ENDS HERE ────────────────────────────────────────────
}

void loop()
{
    // Wait until at least one edge has arrived AND the line has gone quiet for
    // longer than FRAME_GAP — that means the remote finished transmitting.
    if (edge_count == 0)
        return;

    // Snapshot edge_count, wait, see if it grew. If it stopped growing past
    // FRAME_GAP since the last edge, the frame is complete.
    uint16_t count_now = edge_count;
    uint32_t last_edge = edges[count_now - 1];

    if (!buf_overflow && (micros() - last_edge) < FRAME_GAP)
        return; // still receiving, come back later

    // Frame is done. Freeze the buffer: disable the ISR so nothing mutates
    // `edges`/`edge_count` while we decode and print (printing is slow).
    PCMSK2 &= ~(1 << PCINT18);

    decodeFrame(count_now, buf_overflow);

    // Reset for the next button press and re-arm the interrupt.
    edge_count = 0;
    buf_overflow = false;
    PCMSK2 |= (1 << PCINT18);
}
