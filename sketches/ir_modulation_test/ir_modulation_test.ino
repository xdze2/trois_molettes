// IR modulation test — ATmega328PB at 4 MHz effective (CKDIV8 fuse active)
//
// Sends a ~475 µs burst at 38 kHz every second so you can verify:
//   CH1: TSOP38238 OUT pin → active-low dip during burst
//   CH2: IR_PIN (or transistor collector) → 38 kHz carrier during burst
//
// Timing note: IDE compiles for F_CPU=8000000 but chip runs at 4 MHz.
// delayMicroseconds() runs 2× slower than requested, so we ask for half
// the desired delay. 6 µs requested → ~13 µs actual → ~38 kHz carrier.

#define IR_PIN 3 // OC2B — change if needed

// One 38 kHz carrier cycle: ~26 µs total at 4 MHz effective.
// We request 6 µs per half-period (executes as ~13 µs at actual 4 MHz).
#define HALF_PERIOD_US 4 // measured freq is approx 36kHz

// 475 µs burst = ~18 carrier cycles at 38 kHz
#define BURST_CYCLES 18

void sendBurst()
{
  for (int i = 0; i < BURST_CYCLES; i++)
  {
    digitalWrite(IR_PIN, HIGH);
    delayMicroseconds(HALF_PERIOD_US);
    digitalWrite(IR_PIN, LOW);
    delayMicroseconds(HALF_PERIOD_US);
  }
}

void setup()
{
  pinMode(IR_PIN, OUTPUT);
  digitalWrite(IR_PIN, LOW);

  // Serial: request 4800, actual baud is 2400 due to CKDIV8 (see howtos/02_serial_debug.md)
  Serial.begin(4800);
  Serial.println("IR LED test — burst every 1 s");
}

void loop()
{
  sendBurst();
  Serial.println("burst");
  delay(1000);
}
