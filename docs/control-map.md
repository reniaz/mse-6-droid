# Control Map

How the transmitter drives the droid's lights and sound. Channels 1–3 stay with the drive gear and never reach the microcontroller.

## Channels

| RC ch | Pro Mini | Type | Function |
| :-- | :-- | :-- | :--- |
| 10 | D8 | Button | Random category from `01`–`04`, then a random file inside it |
| 9 | D7 | 3-pos switch | Mode select — see [Modes](#modes) |
| 8 | D6 | Button | Random from `01` (General) · held → Imperial March |
| 7 | D5 | Button | Random from `02` (Alert) · held → Cantina Band |
| 6 | D4 | Button | Random from `03` (Question) · held → Throne Room |
| 5 | D3 | Button | Random from `04` (Runaway) · held → Main Theme |
| 4 | D2 | Knob | Playback volume, 1000 µs lowest → 2000 µs highest |

A held button is one down for longer than the long-press threshold; it plays the music cue instead of the category sound.

## Modes

Channel 9 selects one of three behaviours.

| Position | Pulse | Mode | Behaviour |
| :-: | :-- | :-- | :--- |
| 1 | ~1000 µs | **Normal** | Everything behaves as the channel table describes |
| 2 | ~1500 µs | **Alarm** | Lights active, `05/002.wav` loops |
| 3 | ~2000 µs | **Soundloop** | `05/001.wav` loops; channel 10 toggles the alarm blink on and off, no sound |

## Light Pattern

Whenever the lights run, they follow the screen pattern, timed off a 24 fps render: one bank lit for a single frame, dark for four, then the other bank the same. A frame is 41.7 ms, so 42 ms lit and 167 ms dark, giving a 417 ms cycle.

## Sound Bank

Files live on the DFPlayer's SD card in numbered folders, tracks counting up from `001.wav`.

| Folder | Category | Tracks |
| :-: | :-- | :-- |
| `01` | General | 17 |
| `02` | Alert | 6 |
| `03` | Question | 2 |
| `04` | Runaway | 10 |
| `05` | Loops | `001` sound loop · `002` alarm |
| `06` | Music | `001` Imperial March · `002` Cantina Band · `003` Throne Room · `004` Main Theme |

## Notes

Every input pin above sits on a pin-change interrupt, so pulse widths are timed on the edge rather than polled — button timing stays accurate no matter how many channels are active.

**Outputs and other pins**

| Pin | Use |
| :-- | :--- |
| TX0 (D1) | To the sound module, through a 1 kΩ resistor |
| RX0 (D0) | Back from the sound module |
| D10, D11 | LED banks, switched via transistors |
| D12 | Left floating — ambient noise seeds the random sound picker |

The sound module talks back, so the board knows when a track has finished rather than having to guess. Those two pins are also the programming line: the return wire needs a series resistor, or unplugging, before the board can be reflashed.

**Free pins:** D9, D13 and A0–A7.
