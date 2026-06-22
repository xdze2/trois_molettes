# Serial Debug — ATmega328PB on this board

## The problem: garbled serial output

The board runs at **4 MHz effective clock**, not 8 MHz.

The chip is an ATmega328PB with an 8 MHz crystal, but the **CKDIV8 fuse is active**,
dividing the clock by 2 (the bootloader on this board only partially configures the fuses).
The Arduino IDE compiles with `F_CPU=8000000`, so every baud rate calculation is off by a
factor of 2.

**Working configuration:**

| Setting | Value |
|---|---|
| `Serial.begin()` in sketch | `4800` |
| Serial monitor / `screen` baud rate | `2400` |

The sketch asks for 4800, the hardware produces 2400, the monitor reads 2400 → readable.

## Confirmed by oscilloscope

Probing the TX pin (Arduino pin 1) while sending `0x55` at `Serial.begin(9600)`:

- Expected bit period at 9600 baud, 8 MHz: **104 µs**
- Measured: **~200 µs** → actual baud rate ~5000, consistent with 4 MHz clock

Use [tools/serial_test/serial_test.ino](../tools/serial_test/serial_test.ino) to reproduce
this measurement. Set `BAUD_RATE 9600`, probe TX, measure the shortest pulse width.

## Opening the serial port on macOS

```bash
ls /dev/tty.usb*          # find the port name
screen /dev/tty.usbserial-110 2400
```

To quit `screen`: `Ctrl-A` then `K`, confirm with `Y`.

The port is `/dev/tty.usbserial-110` on this setup (see [01_arduino_setup.md](01_arduino_setup.md)).

**Note:** close the Arduino IDE Serial Monitor before using `screen` — both cannot hold
the port at the same time ("could not find a pty" error).

## Root cause: CKDIV8 fuse

The ATmega328P/PB has a fuse bit called **CKDIV8**. When set, it divides the system clock
by 2 at startup. It is enabled by default on blank chips and must be cleared by burning the
bootloader with an ISP programmer.

The bootloader on this board (old Arduino Pro Mini / STK500v1 protocol) does not clear it,
leaving the chip at 4 MHz instead of 8 MHz.

## Permanent fix: burn the bootloader with an ISP

**What is an ISP?** An In-System Programmer connects directly to the chip's SPI pins
(MOSI, MISO, SCK, RESET) and can rewrite fuses and flash without needing a working
bootloader. Any Arduino board can act as one using the built-in "Arduino as ISP" sketch.

**The bootloader is stored inside the ATmega chip**, not on the board PCB. Burning it
rewrites the chip's fuse bytes and bootloader flash section. The board itself is unaffected.

**Risk:** low, but if the fuses are set incorrectly the chip can become unresponsive to
further ISP programming (recoverable with a high-voltage programmer, but inconvenient).

### Steps (Arduino as ISP)

1. Flash the "Arduino as ISP" sketch onto a second Arduino (File → Examples → ArduinoISP).
2. Wire the ISP connections between the programmer Arduino and the target board:

   | Programmer | Target |
   |---|---|
   | D10 | RESET |
   | D11 (MOSI) | MOSI |
   | D12 (MISO) | MISO |
   | D13 (SCK) | SCK |
   | 5V / 3V3 | VCC |
   | GND | GND |

3. In the IDE: Tools → Board → **Arduino Pro or Pro Mini**, Processor → **ATmega328P (3.3V, 8 MHz)**.
4. Tools → Programmer → **Arduino as ISP**.
5. Tools → **Burn Bootloader**.

After this, the CKDIV8 fuse will be cleared, the chip will run at 8 MHz, and
`Serial.begin(9600)` will produce 9600 baud correctly — no more factor-of-2 workaround needed.
