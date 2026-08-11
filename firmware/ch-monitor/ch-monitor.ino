/*
  Receiver channel monitor — Arduino Pro Mini.

  Prints every channel's pulse width plus a running count of pulses that
  came back outside RC range. A healthy channel holds steady within a few
  microseconds and its reject count stays put; a channel that jitters
  across 1500, or whose reject count climbs, is the one causing trouble.

  Unplug the DFPlayer wire from TX0 before flashing this — the module
  shares that line and would read the output as commands.

  Serial monitor at 115200.
*/

#define PIN_CH4    3
#define PIN_CH5    4
#define PIN_CH6    5
#define PIN_CH7    6
#define PIN_CH8    7
#define PIN_CH9    8
#define PIN_CH10  10

enum { CH_VOL, CH_B5, CH_B6, CH_B7, CH_B8, CH_B9, CH_MODE, CH_COUNT };

const char *LABEL[CH_COUNT] = { "ch4", "ch5", "ch6", "ch7", "ch8", "ch9", "ch10" };

volatile uint16_t pulseWidth[CH_COUNT];
volatile uint32_t riseStamp[CH_COUNT];
volatile uint16_t rejects[CH_COUNT];
volatile uint16_t frames[CH_COUNT];

static uint8_t prevD, prevB;

static inline void capturePulse(uint8_t idx, uint32_t t) {
  uint16_t w = (uint16_t)(t - riseStamp[idx]);
  if (w >= 800 && w <= 2200) {
    pulseWidth[idx] = w;
    frames[idx]++;
  } else {
    rejects[idx]++;
  }
}

ISR(PCINT2_vect) {
  uint8_t now = PIND;
  uint8_t changed = now ^ prevD;
  prevD = now;
  uint32_t t = micros();

  for (uint8_t i = 0; i < 5; i++) {
    uint8_t mask = _BV(PD3 + i);
    if (!(changed & mask)) continue;
    if (now & mask) riseStamp[i] = t;
    else capturePulse(i, t);
  }
}

ISR(PCINT0_vect) {
  uint8_t now = PINB;
  uint8_t changed = now ^ prevB;
  prevB = now;
  uint32_t t = micros();

  if (changed & _BV(PB0)) {
    if (now & _BV(PB0)) riseStamp[CH_B9] = t;
    else capturePulse(CH_B9, t);
  }
  if (changed & _BV(PB2)) {
    if (now & _BV(PB2)) riseStamp[CH_MODE] = t;
    else capturePulse(CH_MODE, t);
  }
}

void setup() {
  pinMode(PIN_CH4, INPUT);  pinMode(PIN_CH5, INPUT);
  pinMode(PIN_CH6, INPUT);  pinMode(PIN_CH7, INPUT);
  pinMode(PIN_CH8, INPUT);  pinMode(PIN_CH9, INPUT);
  pinMode(PIN_CH10, INPUT);

  prevD = PIND;
  prevB = PINB;

  PCICR |= _BV(PCIE2) | _BV(PCIE0);
  PCMSK2 |= _BV(PCINT19) | _BV(PCINT20) | _BV(PCINT21) | _BV(PCINT22) | _BV(PCINT23);
  PCMSK0 |= _BV(PCINT0) | _BV(PCINT2);

  Serial.begin(115200);
  Serial.println();
  Serial.println(F("channel  width  state  frames  rejects"));
}

void loop() {
  for (uint8_t i = 0; i < CH_COUNT; i++) {
    uint16_t w, f, r;
    noInterrupts();
    w = pulseWidth[i];
    f = frames[i];
    r = rejects[i];
    interrupts();

    Serial.print(LABEL[i]);
    Serial.print(F("\t "));
    Serial.print(w);
    Serial.print(F("\t "));
    Serial.print(w > 1500 ? F("ON ") : F("off"));
    Serial.print(F("\t "));
    Serial.print(f);
    Serial.print(F("\t "));
    Serial.println(r);
  }
  Serial.println();
  delay(400);
}
