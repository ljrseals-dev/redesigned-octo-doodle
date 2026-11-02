# Handoff to Grok: CarrierField (17-band carrier-field synth)

Date: 2026-11-02. From: Claude. Project owner: Lewis Seals.

## Goal
A 17-band carrier-field drone instrument (not a conventional oscillator synth): bands pulled into a coherent harmonic body by
Master Gravity, with Phase Field Lock, Harmony Coherence (x20/x100), Cycle Detune, a Chaos section (Mode/Amount/Type),
Freeze EM, a 17 Hz fold, low/mid/high group coherence, TET grids and extreme octave shifts. Drone only: no note or sequencing logic.
The original ZS KMGTP behaviour is unverified; every control meaning below is an interpretation.

## Files
| File | What it is |
|---|---|
| `cosmic-synthesizer-engine-carrierfield.zip` | Full C++20 tree: original engine + new CarrierField, plugin shell, tests, CI, docs |
| `cosmic_carrier_synth_plan.xlsx` | Repo overview, spec mapping, verified code findings, gaps, build plan, verification status |
| `carrier_field_demo.wav` / `_spectrogram.png` | 66 s offline render (free carriers -> gravity -> coherence x20/x100 -> chaos -> freeze 46-58 s -> release) |
| `carrierfield_playground.html` | Browser port (JavaScript, ScriptProcessor) with sliders, Freeze and the same 66 s demo timeline. Untested in a browser |

## Where things are inside the zip
- `Source/DSP/CarrierField/BandDrift.h`: C++ port of the analog-drift model (1/f + random walk + jitter, 1.5 kHz model rate).
- `Source/DSP/CarrierField/CarrierField.h`: the engine. Docs: `Docs/CARRIER_FIELD.md` (interpretations, verification numbers, limits).
- `Source/Plugin/CarrierFieldInstrument.h`: JUCE-free parameter table (74 params), `Controls`, `Instrument`. `CarrierFieldProcessor.{h,cpp}`: thin JUCE shell. Docs: `Docs/PLUGIN.md`.
- `Tests/carrierfield_tests.cpp`, `Tests/carrierfield_plugin_tests.cpp`, `Tools/render_drone.cpp`, `.github/workflows/ci.yml`.
- The original single-oscillator `CosmicSynth` plugin is untouched.

## Build and test (no cmake needed)
```
g++ -std=c++20 -O2 -Wall -Wextra Tests/carrierfield_tests.cpp -o cft && ./cft
g++ -std=c++20 -O2 -Wall -Wextra Tests/carrierfield_plugin_tests.cpp -o pct && ./pct
g++ -std=c++20 -O2 Tools/render_drone.cpp -o render_drone && ./render_drone demo.wav demo.csv
```
Sanitizers: add `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined`. All pass and are clean as of this handoff.

## Interpretations (change only deliberately)
- **Master Gravity**: log-domain frequency pull toward the nearest harmonic of f0/harmonicDivisor, plus phase relaxation toward the harmonic phase.
- **Phase Field Lock**: Kuramoto-style mean-field pull toward each group's circular-mean phase (low 0-5, mid 6-11, high 12-16).
- **Harmony Coherence**: loop-gain multiplier (x1/x20/x100). Pulls are exact exponential relaxations per tick, so high gain is stable; frequency correction is slew-limited to 30%.
- **harmonicDivisor (default 4)**: at 1, only 8 of 17 bands get distinct harmonics (pulls up to 700 cents); at 4: 13 distinct, max 200 cents.
- **17 Hz fold**: per-band triangle phase modulation (not a wavefolder).
- **Freeze EM**: latch frequency, fold depth, phase offset, level and coherence normalisation; 20 ms crossfade; drift keeps running underneath.
- **Chaos Mode**: scales drift depth (x0.25/x1/x3) and loop gain (x1/x1/x0.2). Type picks Flicker/Wander/Jitter/Blend.
- Discontinuous controls (chaos amount, fold, mode, level, phase offset) are smoothed so changes never click. Default master level 0.6; soft-knee limiter above 0.9.

## Verified (numbers)
Drift PSD slopes -1.06 / -1.99 / +0.01 (targets -1/-2/0); seed cross-correlation worst 0.21; coherence 0.24 -> 0.99 as gravity 0 -> 0.75;
lock time (R>0.9) 39 / 26 / 21 ms at gravity 0.5 / 0.75 / 1.0; out-of-line energy -95 to -103 dB without fold; 60 s freeze: model parameters change by 6e-16;
9 parameter transitions click-free; plugin-core fuzz (random jumps, min/max, 22.05-192 kHz) finite and bounded; about 40x real time.

## NOT verified (do not claim otherwise)
1. **Nobody has listened to the output.** Drift depths, presets, gravity feel and loudness need tuning by ear. This is the top item. (Locked field is about 6 dB quieter than free carriers.)
2. **The JUCE shell has not been compiled against real JUCE.** It only compiled and ran against a hand-written stub of the JUCE API. The CI job `juce-plugin` (JUCE 8.0.6) will test it; it is set to continue-on-error until first green.
3. **CMake and the GitHub Actions workflow have never been run** (no cmake or network in the build sandbox).
4. The browser playground is a separate JS port, not the C++ code, and has not been opened in a browser.
5. Not built: custom plugin editor (uses JUCE's generic editor), stereo spread (mono duplicated), polyphony, a true wavefolder, oversampling.

## Open decisions for the owner
- Keep the 17 Hz fold as phase modulation, or replace it with a true wavefolder (adds harmonics, needs oversampling)?
- Preset values after the listening pass.

## Suggested next steps
1. Push to GitHub, run CI, fix whatever `juce-plugin` reports.
2. Owner listens (demo wav, playground, or Standalone build) and reports concrete problems; turn them into default values.
3. Custom editor, then stereo spread, then optional polyphony.

## Notes for working on this
- Keep the real-time contract: header-only DSP, no allocation, locks or I/O in the process path.
- Do not add note or sequencing logic; the engine is deliberately drone-only.
- The two upstream engine zips (`cosmic-vco-engine`, `cosmic-synthesizer-engine`) are identical DSP; this tree is based on the synthesizer-engine one.
- Source repos: ljrseals-dev/terrible-cackle (engine) and ljrseals-dev/infamous-casket (Analog Drift, JavaScript).
