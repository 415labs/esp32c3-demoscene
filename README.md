# 415 SIGNAL: a 2-minute demo for the ESP32-C3 0.42" OLED board

A demo for the ESP32-C3 board with the 72×40 OLED. It runs 9 visual parts against a 128 BPM synth soundtrack that plays from **one GPIO pin**. Both the music and the visuals are generated in real time. Nothing is pre-rendered.

## Wiring

| What | Pin |
|---|---|
| OLED (on-board) | SDA = GPIO5, SCL = GPIO6, addr 0x3C |
| Audio out | **GPIO3** → speaker/headphone → **GND** |

Recommended audio hookups, from simplest to best:

1. **Headphones / earbuds:** GPIO3 → **220 Ω** → **10 µF** (+ toward GPIO) → tip (both L+R) → sleeve to GND.
   The resistor keeps GPIO current below 15 mA and sets the volume. The capacitor blocks the 1.65 V DC idle level.
2. **Small 8 Ω speaker, directly:** GPIO3 → 100 Ω → 10–47 µF → speaker → GND. This works but is quiet.
3. **Louder speaker:** GPIO3 → 1 kΩ → base of an NPN (2N2222/BC547), emitter to GND, speaker between 5 V and the collector, with an optional 10 Ω in series.
   Or feed a PAM8302/PAM8403 amp through a 1 kΩ + 10 nF RC low-pass.

To use a different pin, change `AUDIO_PIN` in `src/main.cpp`. Usable pins are 0, 1, 2, 3, 4, 7, 10, 20 and 21. Avoid 5 and 6 (OLED), 8 (LED) and 9 (BOOT).

## Build & flash (PlatformIO)

```
pio run                 # build
pio run -t upload       # flash over USB-C
pio device monitor      # fps readout (115200)
```

* `platformio.ini` uses the **pioarduino** platform. That platform provides Arduino-ESP32 core 3.x, which the new I2S PDM audio driver requires; the stock `espressif32` platform still ships core 2.x.
* Verified: `pio run` builds cleanly with core 3.3.13, with no warnings. The image is 414 KB and static RAM is 141 KB.
* If upload fails to connect, hold **BOOT**, tap **RESET**, release BOOT, and retry.

Project layout:

```
platformio.ini
src/main.cpp        OLED driver, I2S PDM audio task, main loop, pin config
include/song.h      sequencer + integer synth + visual sync
include/effects.h   the 9 visual parts
include/gfx.h       1-bit framebuffer, 5x7 font, 3D helpers
sim/sim.cpp         desktop simulator (WAV + video preview)
```

## Running order (120 s, loops)

| Bars | Time | Visual | Music | Sync tricks |
|---|---|---|---|---|
| 0–3 | 0:00 | **3D wireframe brilliant-cut diamond** is built one vertex, then one edge, per arp pluck. Then it spins up and zooms through the camera. | Pad swells, filter opens, arp plucks, riser | 1 pluck = 1 edge. White-out lands on the downbeat |
| 4–7 | 0:07 | **415 / LABS vector logo** slams in, wobbles in 3D, then collapses to one line. *PRESENTS* is typed by the hi-hats. | Sub-booms, bass, open hats, snare roll | Kick = zoom punch + screen shake. 1 hat = 1 letter. The snare roll glitches the logo apart |
| 8–15 | 0:15 | **Warp starfield**. Every kick launches a diamond shockwave. | THE DROP: 4/4 kick, acid bass, claps | Kick = warp boost + shockwave + brightness pump |
| 16–23 | 0:30 | **Dithered plasma**. Every melody note drops a ripple at a spot set by its pitch. | The lead melody enters | Snare = screen inversion. Kick = zoom |
| 24–31 | 0:45 | **128-point 3D morph**: sphere → torus → cube → 415LABS → helix → gem… | Full arrangement | Each shape change starts on a downbeat |
| 32–39 | 1:00 | **Breakdown sine scroller** with greetings. The text bounce follows the lead. | No drums. Pad, echoing lead | A dithered ghost copy trails exactly one echo delay behind, matching the audio delay |
| 40–47 | 1:15 | **Rotozoomer** over a "415" texture | Second drop, 16th hats | Kick = zoom-in punch. Snare = inversion |
| 48–55 | 1:30 | **Tunnel** with a wandering centre | Lead an octave up | Kick = travel jump + rings flash + screen shake |
| 56–63 | 1:45 | **The diamond returns** with motion trails. It shatters on the final hit, then logo + THANK YOU fade out | Outro, final crash, pad fade | Fade done with the OLED contrast register |

## How the sync works

* **One clock.** The synth fills 256-sample buffers that the I2S DMA plays out at exactly 32 kHz. Each frame, the render loop works out which sample is leaving the pin *right now*: samples written − DMA queue + µs elapsed since the last write. It then renders the frame for that exact instant.
* **One score.** The synth and the visuals read the same pattern functions (`kickAt`, `snareAt`, `hatAt`, `leadAt`, `chordAt`). The visuals derive their envelopes from the exact sample a drum started on. They never guess or keep separate timers.
* **Stateless effects.** Every part is a pure function of song position. A slow frame can't drift, and a dropped frame doesn't accumulate error.
* **Hardware OLED tricks, at zero render cost:**
  * The **contrast register** pumps with the kick, which acts as a visual side-chain matching the audio side-chain on bass and pad.
  * **Display invert** flashes on snares and crashes.
  * **Start-line offset** shakes the screen vertically on booms.

## Sound engine (integer only; the C3 has no FPU)

* 32 kHz output through the **I2S peripheral in PDM "DAC line" mode**: a ~6 MHz 1-bit stream on one pin.
* Voices:
  * Kick: pitch-swept sine + click
  * Snare: noise + body
  * Hats and crash: high-passed noise
  * Riser: swept filtered noise
  * Acid bass: saw → resonant state-variable filter with envelope and accents
  * PWM lead: glide + delayed vibrato
  * Chip-style arpeggio
  * 6-oscillator detuned saw pad with a 2-pole low-pass
  * Dotted-eighth feedback echo on lead and arp
  * Side-chain ducking from the kick
* Synth CPU is roughly 5% of the core, and it runs in a high-priority FreeRTOS task. Rendering + I2C at 800 kHz gives about 60–100 fps.

## Tuning

* **Image shifted or cut off:** change `OLED_XOFF` (28 is standard for these panels; some use 30 or 32).
* **Display glitches:** lower `I2C_HZ` to 400000.
* **Too loud or too quiet:** the `G_*` gains at the top of the synth section in `song.h` are Q8 (256 = 1.0).
* **Desktop preview:** `sim/sim.cpp` renders the exact same code to WAV + video:

  ```
  g++ -O2 -o sim sim/sim.cpp      # from the project root
  ./sim wav song.wav
  ./sim video 50 8 | ffmpeg -f rawvideo -pix_fmt rgb24 -s 576x320 -r 50 -i - -i song.wav -shortest out.mp4
  ```
