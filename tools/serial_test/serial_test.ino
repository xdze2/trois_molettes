// Serial connection test — scope / baud rate diagnosis
//
// Sends a repeating pattern at a fixed baud rate so you can measure
// the actual bit period on an oscilloscope and confirm the baud rate.
//
// TX pin: Arduino pin 1 (PD1) — probe here with the scope.
//
// Useful waveforms:
//   0x55 (01010101) → alternating bits, best for measuring bit period
//   0xAA (10101010) → same but inverted
//   0x00            → long LOW pulse (measure full byte = 10 bit-periods incl. start/stop)
//
// Bit period = 1 / baud rate.  At 9600 baud → 104 µs per bit.

#define BAUD_RATE  4800

void setup()
{
    Serial.begin(BAUD_RATE);
}

void loop()
{
    // 0x55 = alternating bits: easy to measure bit period on a scope
    Serial.write(0x55);
    Serial.println(F("[led] ON"));
    delay(10);
}
