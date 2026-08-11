/*
  MSE-6 lights and sound controller — Arduino Pro Mini (ATmega328P).
  Wiring and behaviour: docs/control-map.md

  Audio is DFRobotDFPlayerMini on the hardware UART, wired both ways:
  TX0 -> module RX through 1k, module TX -> D0.

  D0 is also the programming line. With the module driving it, uploads
  will fail to sync unless that wire has a series resistor (1k) so the
  programmer can win the line, or is unplugged while flashing.

  The library runs with ACK off, so a send blocks around 10ms instead of
  waiting for a reply. Button edges are latched in the pin-change handler,
  so a stall that long costs nothing.
*/

#include <DFRobotDFPlayerMini.h>

DFRobotDFPlayerMini df;

#define PIN_CH4    2    // volume knob
#define PIN_CH5    3    // button - folder 04 / Main Theme
#define PIN_CH6    4    // button - folder 03 / Throne Room
#define PIN_CH7    5    // button - folder 02 / Cantina Band
#define PIN_CH8    6    // button - folder 01 / Imperial March
#define PIN_CH9    7    // 3-position mode switch
#define PIN_CH10   8    // button - random across folders 01-04

#define PIN_LED_A 10
#define PIN_LED_B 11
#define PIN_SEED  12    // left floating, harvested for RNG entropy

#define LONG_PRESS_MS   350
#define SAMPLE_MS       5     // RC frames arrive at 50Hz; 200Hz is plenty
#define DEAD_SAMPLES    100   // SAMPLE_MS * this = link considered lost
#define VOL_SEND_MS     150
#define VOL_DEADBAND_US 100   // knob must move this far once before it takes over

// Asymmetric on purpose: react on the first valid high so taps are quick,
// but make a release earn it, so a dropped or noisy frame cannot fake one.
#define PRESS_FRAMES    1
#define RELEASE_FRAMES  3

// Long enough to sweep past the middle detent without Alarm firing on the
// way from Normal to Soundloop. Costs the same delay on a real change.
#define MODE_STABLE_MS  250

// The module takes 200-400ms between accepting a play command and actually
// starting the file, and ignores anything sent during that seek.
#define PLAY_GAP_MS     350

#define DF_VOLUME_MAX   30    // module's own scale, not a percentage
#define DEFAULT_VOLUME  6     // 20% of DF_VOLUME_MAX

#define BOOT_FLASHES    5
#define BOOT_FLASH_MS   120

// TX0 is shared with the programmer, so the sketch stays off the line for
// this long after boot. Without it, a reset caught slightly too late
// leaves the sketch talking over avrdude's handshake and the upload dies
// with "not in sync". The boot flash runs inside this window.
#define BOOT_QUIET_MS   1500

// Screen pattern, measured off a 24fps render: one frame lit, four dark,
// then the other bank the same. A frame is 1000/24 = 41.67ms.
#define FRAME_ON_MS     42    // 1 frame
#define FRAME_OFF_MS    167   // 4 frames

/*
  Keep this above every function: the Arduino builder generates its own
  prototypes and injects them at the top of the file, so a type used in
  any signature must be declared before the first function definition or
  the build fails with "Mode does not name a type".
*/
enum Mode { MODE_NORMAL, MODE_ALARM, MODE_SOUNDLOOP };
Mode mode = MODE_NORMAL;

// Indices 0-5 must stay in this order: they map to PD2..PD7 so one ISR
// can sweep them with a single shifted mask.
enum {
  CH_VOL,      // ch4  - D2
  CH_BTN_5,    // ch5  - D3
  CH_BTN_6,    // ch6  - D4
  CH_BTN_7,    // ch7  - D5
  CH_BTN_8,    // ch8  - D6
  CH_MODE,     // ch9  - D7
  CH_BTN_10,   // ch10 - D8
  CH_COUNT
};

/*
  Pulses are timed off Timer1, free-running at 0.5us per tick. Reading
  TCNT1 is two instructions where micros() is closer to forty, which
  matters in a handler that fires on every edge of seven channels. It also
  keeps pulse timing independent of Timer0, which millis and the serial
  code are already using. The 16-bit counter wraps every 32.7ms, longer
  than an RC frame, so the subtraction stays valid across a wrap.
*/
volatile uint16_t pulseWidth[CH_COUNT];   // microseconds
volatile uint16_t riseTicks[CH_COUNT];
volatile uint8_t  frameTick[CH_COUNT];    // bumped on every accepted pulse

// One bit per channel. Maintained entirely in the ISR so button edges are
// caught on the RC frame they happen, not whenever the main loop next
// looks. The latches stay set until consumed, so a tap that starts and
// ends while the loop is busy still gets acted on.
volatile uint8_t stableOn;
volatile uint8_t pressLatch;
volatile uint8_t releaseLatch;
static   uint8_t agree[CH_COUNT];

static uint8_t prevD, prevB;

/*
  An edge missed while the main loop has interrupts off leaves a stale
  rise timestamp, and the next falling edge then measures a nonsense
  width. Discarding anything outside RC range keeps the last good value
  in place instead of letting one bad frame reach the logic.
*/
static inline void capturePulse(uint8_t idx, uint16_t t) {
  uint16_t w = (uint16_t)(t - riseTicks[idx]) >> 1;   // ticks are 0.5us
  if (w < 800 || w > 2200) return;

  pulseWidth[idx] = w;
  frameTick[idx]++;

  uint8_t bit = _BV(idx);
  bool on = w > 1500;
  if (on == (bool)(stableOn & bit)) {
    agree[idx] = 0;
    return;
  }
  if (++agree[idx] < (on ? PRESS_FRAMES : RELEASE_FRAMES)) return;

  agree[idx] = 0;
  if (on) { stableOn |= bit;  pressLatch |= bit; }
  else    { stableOn &= ~bit; releaseLatch |= bit; }
}

// PD2..PD7 - channels 4 through 9
ISR(PCINT2_vect) {
  uint16_t t = TCNT1;
  uint8_t now = PIND;
  uint8_t changed = now ^ prevD;
  prevD = now;

  for (uint8_t i = 0; i < 6; i++) {
    uint8_t mask = _BV(PD2 + i);
    if (!(changed & mask)) continue;
    if (now & mask) riseTicks[i] = t;
    else capturePulse(i, t);
  }
}

// PB0 (D8) - channel 10. PB2/PB3 are the LED outputs and stay masked off.
ISR(PCINT0_vect) {
  uint16_t t = TCNT1;
  uint8_t now = PINB;
  uint8_t changed = now ^ prevB;
  prevB = now;

  if (changed & _BV(PB0)) {
    if (now & _BV(PB0)) riseTicks[CH_BTN_10] = t;
    else capturePulse(CH_BTN_10, t);
  }
}

struct ChannelSnapshot {
  uint16_t us;
  bool live;
};
ChannelSnapshot chan[CH_COUNT];

/*
  Rate limited on purpose. Buttons do not depend on this at all - the ISR
  latches their edges - and mode and volume are read from widths that only
  change once per RC frame. Running it every pass meant seven brief
  interrupt-disabled windows tens of thousands of times a second, which
  starves the very handler doing the measuring.
*/
void sampleChannels(uint32_t nowMs) {
  static uint32_t lastAt = 0;
  static uint8_t seenTick[CH_COUNT];
  static uint8_t missed[CH_COUNT];

  if (nowMs - lastAt < SAMPLE_MS) return;
  lastAt = nowMs;

  for (uint8_t i = 0; i < CH_COUNT; i++) {
    uint16_t w;
    noInterrupts();
    w = pulseWidth[i];
    interrupts();
    chan[i].us = w;

    uint8_t tick = frameTick[i];   // single byte, atomic on AVR
    if (tick != seenTick[i]) {
      seenTick[i] = tick;
      missed[i] = 0;
    } else if (missed[i] < 255) {
      missed[i]++;
    }

    chan[i].live = (w != 0) && (missed[i] < DEAD_SAMPLES);

    // A dropped link must not leave a button stuck down.
    if (!chan[i].live) {
      uint8_t bit = _BV(i);
      noInterrupts();
      if (stableOn & bit) {
        stableOn &= ~bit;
        releaseLatch |= bit;
      }
      interrupts();
    }
  }
}

static bool takeEdge(volatile uint8_t &latch, uint8_t ch) {
  uint8_t bit = _BV(ch);
  bool got;
  noInterrupts();
  got = latch & bit;
  latch &= ~bit;
  interrupts();
  return got;
}

bool takePress(uint8_t ch)   { return takeEdge(pressLatch, ch); }
bool takeRelease(uint8_t ch) { return takeEdge(releaseLatch, ch); }

const uint8_t FOLDER_TRACKS[5] = { 0, 17, 6, 2, 10 };   // [0] unused

#define FOLDER_LOOPS  5
#define TRACK_LOOP    1
#define TRACK_ALARM   2
#define FOLDER_MUSIC  6

struct ButtonSpec {
  uint8_t channel;
  uint8_t folder;
  uint8_t musicTrack;
};

const ButtonSpec BUTTONS[4] = {
  { CH_BTN_5, 4, 4 },
  { CH_BTN_6, 3, 3 },
  { CH_BTN_7, 2, 2 },
  { CH_BTN_8, 1, 1 },
};

uint8_t lastFolder = 0, lastTrack = 0;
uint8_t pendFolder = 0, pendTrack = 0;

/*
  The library only spaces commands by 10ms and the module drops the second
  of a close pair. Documented minimum is 100ms; 120 leaves margin. A play
  gets the whole seek window before anything follows it.

  No loop command is ever sent. The module's repeat flag is a latch, and a
  dropped clear leaves every later sound repeating forever - looping is
  driven by end-of-track reports instead, which cannot get stuck on.
*/
#define DF_CMD_GAP    120
#define DF_SETTLE_MS  800   // after reset, before the module accepts anything
#define DF_QUEUE_LEN  8

enum { DF_STOP, DF_PLAY, DF_VOLUME };

struct DfAction { uint8_t cmd, p1, p2; };
DfAction dfQ[DF_QUEUE_LEN];
uint8_t dfHead = 0, dfTail = 0;
uint32_t dfNextAt = 0;

// No default arguments: the Arduino builder copies this signature into a
// generated prototype, and defaults appearing twice is a compile error.
void dfPush(uint8_t cmd, uint8_t p1, uint8_t p2) {
  uint8_t next = (uint8_t)((dfHead + 1) % DF_QUEUE_LEN);
  if (next == dfTail) return;
  dfQ[dfHead].cmd = cmd;
  dfQ[dfHead].p1 = p1;
  dfQ[dfHead].p2 = p2;
  dfHead = next;
}

bool dfIdle() { return dfHead == dfTail; }
void dfFlush() { dfHead = dfTail = 0; }

void serviceDf(uint32_t now) {
  if (dfIdle()) return;
  if ((int32_t)(now - dfNextAt) < 0) return;

  DfAction a = dfQ[dfTail];
  dfTail = (uint8_t)((dfTail + 1) % DF_QUEUE_LEN);

  uint16_t gap = DF_CMD_GAP;
  switch (a.cmd) {
    case DF_STOP:   df.stop();       break;
    case DF_VOLUME: df.volume(a.p1); break;
    case DF_PLAY:
      df.playFolder(a.p1, a.p2);
      gap = PLAY_GAP_MS;
      break;
  }
  dfNextAt = now + gap;
}

// Latest wins: mashing buttons plays what was pressed last rather than
// working through a backlog.
void queueTrack(uint8_t folder, uint8_t track) {
  lastFolder = folder;
  lastTrack = track;
  pendFolder = folder;
  pendTrack = track;
}

// Held back until the queue drains so a button sound cannot overtake the
// stop and loop-off a mode change just issued.
void servicePlayback() {
  if (pendFolder == 0) return;
  if (!dfIdle()) return;
  dfPush(DF_PLAY, pendFolder, pendTrack);
  pendFolder = 0;
}

/*
  Sole loop mechanism: the track ends, we start it again. Costs a seek gap
  between repeats, but cannot latch on the way the module's repeat flag
  can, and cannot fight it either.
*/
void serviceIncoming(uint32_t now) {
  if (!df.available()) return;
  if (df.readType() != DFPlayerPlayFinished) return;

  // A stop or a fresh play makes the module report the previous track as
  // finished. Ignoring reports inside the settling window stops that from
  // counting as a loop cycle and firing a second play.
  if ((int32_t)(now - dfNextAt) < 0) return;

  if (mode == MODE_ALARM)          dfPush(DF_PLAY, FOLDER_LOOPS, TRACK_ALARM);
  else if (mode == MODE_SOUNDLOOP) dfPush(DF_PLAY, FOLDER_LOOPS, TRACK_LOOP);
}

void playRandomFrom(uint8_t folder) {
  uint8_t n = FOLDER_TRACKS[folder];
  uint8_t t = random(n) + 1;
  if (n > 1 && folder == lastFolder && t == lastTrack) t = random(n) + 1;
  queueTrack(folder, t);
}

// Folder first, then a file inside it: every category comes up equally
// often regardless of how many clips it holds.
void playRandomAny() {
  playRandomFrom(random(1, 5));
}

/*
  Entropy from the floating pin: with no pullup it drifts on ambient EM
  pickup. Sampled at 97us, deliberately not a divisor of the 50/60Hz mains
  period, so the reads do not lock to one phase of the hum and return the
  same pattern every boot.
*/
uint32_t harvestSeed() {
  uint32_t seed = micros();
  for (uint8_t i = 0; i < 64; i++) {
    seed = (seed << 1) | (seed >> 31);
    seed ^= (uint32_t)digitalRead(PIN_SEED) ^ (micros() & 0x0F);
    delayMicroseconds(97);
  }
  return seed;
}

void setBanks(bool a, bool b) {
  digitalWrite(PIN_LED_A, a);
  digitalWrite(PIN_LED_B, b);
}

void setLights(bool on) { setBanks(on, on); }

void bootFlash() {
  for (uint8_t i = 0; i < BOOT_FLASHES; i++) {
    setLights(true);
    delay(BOOT_FLASH_MS);
    setLights(false);
    delay(BOOT_FLASH_MS);
  }
}

/*
  Four steps: bank A one frame, dark four frames, bank B one frame, dark
  four frames. Ten frames per cycle, 417ms. Step 3 is the reset state so
  the first update advances into step 0 and lights bank A straight away.
*/
uint8_t blinkStep = 3;
uint32_t blinkChangedAt = 0;

void resetBlink() {
  blinkStep = 3;
  blinkChangedAt = 0;   // next update starts the pattern immediately
  setBanks(false, false);
}

void updateBlink(uint32_t now) {
  uint16_t span = (blinkStep & 1) ? FRAME_OFF_MS : FRAME_ON_MS;
  if (now - blinkChangedAt < span) return;

  blinkStep = (blinkStep + 1) & 3;
  blinkChangedAt = now;
  setBanks(blinkStep == 0, blinkStep == 2);
}

// enterMode restarts audio, so the switch has to settle before it counts.
Mode readMode(uint32_t nowMs) {
  static Mode candidate = MODE_NORMAL;
  static Mode confirmed = MODE_NORMAL;
  static uint32_t since = 0;

  Mode raw;
  uint16_t us = chan[CH_MODE].us;
  if (!chan[CH_MODE].live) raw = MODE_NORMAL;
  else if (us < 1250)      raw = MODE_NORMAL;
  else if (us < 1750)      raw = MODE_ALARM;
  else                     raw = MODE_SOUNDLOOP;

  if (raw != candidate) {
    candidate = raw;
    since = nowMs;
  } else if (nowMs - since >= MODE_STABLE_MS) {
    confirmed = raw;
  }

  return confirmed;
}

struct ButtonState {
  bool down;
  uint32_t downAt;
  bool longFired;
};
ButtonState btn[4];
bool soundloopBlink = false;

void resetButtons() {
  for (uint8_t i = 0; i < 4; i++) { btn[i].down = false; btn[i].longFired = false; }
  noInterrupts();
  pressLatch = 0;
  releaseLatch = 0;
  interrupts();
}

void enterMode(Mode next) {
  mode = next;
  resetButtons();
  resetBlink();
  soundloopBlink = false;
  pendFolder = 0;   // a queued button sound must not fire into the new mode
  dfFlush();        // nor may anything still waiting from the old one

  dfPush(DF_STOP, 0, 0);   // silence whatever the old mode was playing

  if (next == MODE_ALARM)          dfPush(DF_PLAY, FOLDER_LOOPS, TRACK_ALARM);
  else if (next == MODE_SOUNDLOOP) dfPush(DF_PLAY, FOLDER_LOOPS, TRACK_LOOP);
}

/*
  Music fires the instant the hold threshold passes, the category clip on
  release — otherwise a tap could not be told from the start of a hold.
  A tap short enough that both edges landed while the loop was busy
  arrives here as press and release together, and still plays.
*/
void handleNormal(uint32_t now) {
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t ch = BUTTONS[i].channel;

    if (takePress(ch)) {
      btn[i].down = true;
      btn[i].downAt = now;
      btn[i].longFired = false;
    }

    if (btn[i].down && !btn[i].longFired && now - btn[i].downAt >= LONG_PRESS_MS) {
      queueTrack(FOLDER_MUSIC, BUTTONS[i].musicTrack);
      btn[i].longFired = true;
    }

    if (takeRelease(ch)) {
      if (btn[i].down && !btn[i].longFired) playRandomFrom(BUTTONS[i].folder);
      btn[i].down = false;
    }
  }

  if (takePress(CH_BTN_10)) playRandomAny();
  takeRelease(CH_BTN_10);
}

void handleAlarm(uint32_t now) {
  updateBlink(now);
}

// Channel 10 toggles the alarm blink on and off rather than holding it.
void handleSoundloop(uint32_t now) {
  if (takePress(CH_BTN_10)) {
    soundloopBlink = !soundloopBlink;
    if (!soundloopBlink) resetBlink();
  }
  takeRelease(CH_BTN_10);

  if (soundloopBlink) updateBlink(now);
}

/*
  DEFAULT_VOLUME holds until the knob is physically moved past the
  deadband; from that point the knob tracks continuously for the rest of
  the session. Without the latch the knob's resting position would
  overwrite the default within a frame or two of the receiver waking up.
*/
void updateVolume(uint32_t now) {
  static uint32_t sentAt = 0;
  static uint16_t restUs = 0;
  static bool captured = false;
  static bool tracking = false;
  static int8_t sent = -1;

  if (!chan[CH_VOL].live) return;

  uint16_t us = constrain(chan[CH_VOL].us, 1000, 2000);

  if (!captured) {
    restUs = us;
    captured = true;
    return;
  }

  if (!tracking) {
    int16_t moved = (int16_t)us - (int16_t)restUs;
    if (moved < 0) moved = -moved;
    if (moved <= VOL_DEADBAND_US) return;
    tracking = true;
  }

  if (now - sentAt < VOL_SEND_MS) return;

  int8_t v = (int8_t)map(us, 1000, 2000, 0, DF_VOLUME_MAX);
  if (v == sent) return;

  dfPush(DF_VOLUME, (uint8_t)v, 0);
  sent = v;
  sentAt = now;
}

void setup() {
  pinMode(PIN_CH4, INPUT);  pinMode(PIN_CH5, INPUT);
  pinMode(PIN_CH6, INPUT);  pinMode(PIN_CH7, INPUT);
  pinMode(PIN_CH8, INPUT);  pinMode(PIN_CH9, INPUT);
  pinMode(PIN_CH10, INPUT);

  pinMode(PIN_LED_A, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  setLights(false);

  pinMode(PIN_SEED, INPUT);   // no pullup - the pin must be left floating
  randomSeed(harvestSeed());

  TCCR1A = 0;
  TCCR1B = _BV(CS11);   // /8 prescaler -> 0.5us per tick
  TCNT1 = 0;

  prevD = PIND;
  prevB = PINB;

  PCICR |= _BV(PCIE2) | _BV(PCIE0);
  PCMSK2 |= _BV(PCINT18) | _BV(PCINT19) | _BV(PCINT20)
          | _BV(PCINT21) | _BV(PCINT22) | _BV(PCINT23);
  PCMSK0 |= _BV(PCINT0);

  bootFlash();
  while (millis() < BOOT_QUIET_MS) delay(10);

  Serial.begin(9600);

  // ACK off keeps sends from waiting on a reply; the reset still waits for
  // the module's online message, which it can send now that TX is wired.
  df.begin(Serial, false, true);

  // begin() can return before the module is really ready, and anything
  // sent in that window is lost - including the volume, which is why it
  // came up at the module's own level instead of ours.
  dfNextAt = millis() + DF_SETTLE_MS;

  enterMode(MODE_NORMAL);                 // queues the initial stop
  dfPush(DF_VOLUME, DEFAULT_VOLUME, 0);   // behind them, so it is not flushed
}

void loop() {
  uint32_t now = millis();

  sampleChannels(now);

  Mode wanted = readMode(now);
  if (wanted != mode) enterMode(wanted);

  switch (mode) {
    case MODE_NORMAL:    handleNormal(now);    break;
    case MODE_ALARM:     handleAlarm(now);     break;
    case MODE_SOUNDLOOP: handleSoundloop(now); break;
  }

  serviceIncoming(now);
  servicePlayback();
  updateVolume(now);
  serviceDf(now);
}
