<div align="center">

# MSE-6

**A scratch-built, radio-controlled repair droid**

*The small "mouse droid" that scurries down the corridors of the Death Star.*

<br>

![Platform](https://img.shields.io/badge/platform-Arduino-00979D?style=flat-square&logo=arduino&logoColor=white)
![Language](https://img.shields.io/badge/code-C%2B%2B-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Control](https://img.shields.io/badge/control-RC%20PWM-6E4AFF?style=flat-square)
![Status](https://img.shields.io/badge/status-in%20progress-F5A623?style=flat-square)

</div>

---

Everything here is self-made — the chassis, the electronics, the wiring, and the code that ties it together. This repository holds the software that gives the droid its voice and its lights, along with the build notes as it comes together.

<br>

## The Idea

The MSE-6 is a background prop with almost no screen time, which makes it a great build target. There's no kit to follow and no instruction manual, so every part of it has to be worked out from scratch.

Getting one to actually drive, react, and make the right noises means solving a bit of everything at once — mechanics, electronics, control logic, and the finishing touches that sell the illusion.

<br>

## What It Does

|     |     |
| :-- | :-- |
| **Drives** | Proportional throttle and steering, straight off the radio link |
| **Talks** | A full bank of droid sounds — idle chatter, alerts, questions, panicked squealing, and music |
| **Lights** | Lighting effects switched from the transmitter |
| **Roams** | Fully untethered, running on its own battery |

Driving is handled directly by the radio gear. The code takes care of the personality — sound and light, triggered live from spare channels on the transmitter.

<br>

## Under the Hood

| Part | Role |
| :--- | :--- |
| **Arduino Pro Mini** | Main controller |
| **DFPlayer Mini** | Sound playback |
| **HOTRC F-10A** | 10-channel PWM receiver |
| **Custom chassis & shell** | The droid itself |

<br>

## Status

> **Work in progress.** The shell is built and painted — the software is now coming up channel by channel.

Expect this page to grow alongside the build — photos, wiring notes, and a proper feature list are on the way.

**Roadmap**

|     | Milestone |
| :-: | :--- |
| ✅ | Finished shell and paint |
| 🔧 | Sound triggering across the full audio bank |
| ⬜ | Lighting effects |
| ⬜ | Build photos and a short demo video |

<sub>✅ done &nbsp;·&nbsp; 🔧 in progress &nbsp;·&nbsp; ⬜ planned</sub>

<br>

<div align="center">

Built and maintained by **[reniaz](https://github.com/reniaz)**

</div>
