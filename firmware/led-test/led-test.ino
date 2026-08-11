/*
  LED driver bench test — Arduino Pro Mini.

  Cycles the two LED banks so the driver board can be checked without the
  receiver or the sound module connected. Watch the sequence:

    A on  ->  D11 side works
    B on  ->  D12 side works
    both  ->  neither is stealing the other's current
    off   ->  neither is leaking through

  Anything that never lights, or lights when it should be off, is on the
  board rather than in the firmware.

  No Serial here on purpose: TX0 feeds the DFPlayer, and debug output
  would be read as garbage commands.
*/

#define PIN_LED_A 11
#define PIN_LED_B 12
#define STEP_MS   1200

void setup() {
  pinMode(PIN_LED_A, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
}

void loop() {
  digitalWrite(PIN_LED_A, HIGH);
  digitalWrite(PIN_LED_B, LOW);
  delay(STEP_MS);

  digitalWrite(PIN_LED_A, LOW);
  digitalWrite(PIN_LED_B, HIGH);
  delay(STEP_MS);

  digitalWrite(PIN_LED_A, HIGH);
  digitalWrite(PIN_LED_B, HIGH);
  delay(STEP_MS);

  digitalWrite(PIN_LED_A, LOW);
  digitalWrite(PIN_LED_B, LOW);
  delay(STEP_MS);
}
