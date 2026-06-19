First setup

- Usb to TTL: CH340N
- driver are builtin for macos
- look for `	/dev/tty.usbserial-110`

- Alim using 3V3 from the usb
- Tx->Rx, Rx->Tx

Arduino ide config:
- Board: Arduino Nano
- Procespr: ATmega328P (Old Bootloader)

![](howtos/arduino_ide_config.png)

from
https://forum.arduino.cc/t/bad-cpu-type-in-executable/1391501/15


Press reset on the board when "Uploading..." shows up

Demo using the led on the board (L):
```
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);

  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}
```