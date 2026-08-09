# Control Map

How the transmitter drives the droid's lights and sound. Channels 1–3 stay with the drive gear and never reach the microcontroller.

## Channels

| RC ch | Pro Micro | Type | Function |
| :-- | :-- | :-- | :--- |
| 10 | D10 | 3-pos switch | Mode select — see [Modes](#modes) |
| 9 | D8 | Button | Random sound from folders `01`–`04`, pooled across all files |
| 8 | D7 | Button | Random from `01` (General) · held → Imperial March |
| 7 | D6 | Button | Random from `02` (Alert) · held → Cantina Band |
| 6 | D5 | Button | Random from `03` (Question) · held → Throne Room |
| 5 | D4 | Button | Random from `04` (Runaway) · held → Main Theme |
| 4 | D3 | Knob | Playback volume, 1000 µs lowest → 2000 µs highest |

A held button is one down for longer than the long-press threshold; it plays the music cue instead of the category sound.

## Modes

Channel 10 selects one of three behaviours.

| Position | Pulse | Mode | Behaviour |
| :-: | :-- | :-- | :--- |
| 1 | ~1000 µs | **Normal** | Everything behaves as the channel table describes |
| 2 | ~1500 µs | **Alarm** | Lights active, `05/002.wav` loops |
| 3 | ~2000 µs | **Soundloop** | `05/001.wav` loops; channel 9 fires the lights with no sound |

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

## Unassigned

D9 and D2 are free. D0/D1 are the hardware UART.
