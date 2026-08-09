# MSE-6

A scratch-built, radio-controlled MSE-6 repair droid — the small "mouse droid" that scurries down the corridors of the Death Star in *Star Wars*.

Everything here is self-made: the chassis, the electronics, the wiring and the code that ties it together. This repository is where the control software and build notes live.

## The Idea

The MSE-6 is a background prop with almost no screen time, which makes it a great build target — there is no kit to follow and no instruction manual, so every part of it has to be worked out from scratch. Getting one to actually drive, react and make the right noises means solving a bit of everything: mechanics, electronics, control logic and the finishing touches that sell the illusion.

## What It Does

- Drives under radio control, with proportional steering and throttle
- Plays back droid sounds and audio cues on command
- Spare control channels reserved for lighting and future effects
- Runs untethered on its own battery

## Under the Hood

| Part | Role |
| --- | --- |
| Arduino Pro Micro | Main controller |
| DFPlayer Mini | Sound playback |
| 10-channel RC receiver (PWM) | Remote control link |
| Custom chassis and shell | The droid itself |

## Status

Work in progress. The hardware side is taking shape and the control software is being built up channel by channel. Expect this page to grow as the build does — photos, wiring notes and a proper feature list are on the way.

## Roadmap

- [ ] Reliable drive control and mixing
- [ ] Sound triggering tied to control input
- [ ] Lighting effects
- [ ] Finished shell and paint
- [ ] Build photos and a short demo video

---

Built and maintained by [reniaz](https://github.com/reniaz).
