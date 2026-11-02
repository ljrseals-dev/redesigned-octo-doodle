# CarrierField: Steinberg VST3 plugin (SDK build, no JUCE)

Instrument (`Instrument|Synth`), stereo or mono output, MIDI note-on sets the fundamental. The DSP is the same header-only
CarrierField engine used everywhere else; this folder only adds the VST3 wrapper.

## Layout
```
vst3/
  CMakeLists.txt            SDK-based build (you supply VST3_SDK_ROOT)
  source/
    CarrierFieldVst3Bridge.h  SDK-free logic: parameter conversion, text, state format, sample-accurate event Core (unit-tested)
    CarrierFieldProcessor.*   thin AudioEffect adapter (buses, process(), setState/getState)
    CarrierFieldController.*  thin EditController adapter (74 parameters, strings, component-state restore)
    entry.cpp                 factory + class registration
    cids.h version.h          FUIDs (never change after release) and version/vendor strings
  resource/
    moduleinfo.json           reference module info (newer SDKs generate this automatically; prefer the generated one)
    win32resource.rc          Windows version resource
../Source/DSP/CarrierField/   engine (CarrierField.h, BandDrift.h)   ../Source/Plugin/CarrierFieldInstrument.h  parameter table + Instrument
```
No `public.sdk` / `pluginterfaces` files are copied or modified; they come from your SDK checkout.

## Build
1. Get the SDK: `git clone --recursive https://github.com/steinbergmedia/vst3sdk.git` (you accept Steinberg's licence).
2. `cmake -S vst3 -B build-vst3 -DVST3_SDK_ROOT=/path/to/vst3sdk -DCMAKE_BUILD_TYPE=Release`
3. `cmake --build build-vst3 --config Release`
4. The bundle is `build-vst3/VST3/Release/CarrierFieldVst3.vst3` (macOS builds arm64 + x86_64; Windows x64; Linux works if the SDK builds there).

## Validate
- Run the SDK validator on the bundle: `build-vst3/bin/Release/validator build-vst3/VST3/Release/CarrierFieldVst3.vst3` (path varies by generator).
- Optionally load it in the SDK's `VST3PluginTestHost` or `AudioHost` example, then in a DAW (Cubase, Live, Bitwig, Reaper, Studio One, FL). AU for Logic needs a separate wrapper, not included.
- Never run on this code so far: see "Verification status".

## Behaviour
- Drone: always sounding unless `Drone On` is off (10 ms fade). MIDI note-on sets the fundamental when `Follow MIDI Note` is on; no note-off, no polyphony, no sequencing.
- Automation is sample-accurate: queued parameter points and note events are applied at their sample offsets (tested against manual block splitting). Discontinuous controls are smoothed inside the DSP.
- Output: mono duplicated to both channels (accepts a mono bus too). 32-bit float only.
- State: versioned binary (`CFv1`, count, plain float per parameter). Restores all 74 parameters including Freeze. Older states with fewer parameters load with defaults for the rest; corrupt or truncated state is rejected without changing anything.
- Editor: none; hosts show their generic parameter editor. Parameter units/groups are not defined (flat list).
- process() does not allocate, lock or do I/O. setState (UI thread) hands values to process() through an atomic flag.

## Parameters (global; plus per band 1-17: `bNN_on`, `bNN_level`, `bNN_trim`)
| ID | String id | Name | Type | Range / choices | Default |
|---|---|---|---|---|---|
| 0 | `fundamental` | Fundamental (Hz) | float | 10 to 2000 | 55.0 Hz |
| 1 | `octave` | Octave Shift | int | -8 to 8 | 0  |
| 2 | `tet` | TET Divisions | int | 1 to 96 | 12  |
| 3 | `stride` | Band Step (0=auto) | int | 0 to 96 | 0  |
| 4 | `span` | Band Span (oct) | int | 1 to 10 | 4  |
| 5 | `harmdiv` | Harmonic Divisor | int | 1 to 16 | 4  |
| 6 | `gravity` | Master Gravity | float | 0 to 1 | 0.500  |
| 7 | `pfl` | Phase Field Lock | float | 0 to 1 | 0.000  |
| 8 | `coherence` | Harmony Coherence | list | x1|x20|x100 | x1  |
| 9 | `grp_low` | Low Group Coherence | float | 0 to 2 | 1.000  |
| 10 | `grp_mid` | Mid Group Coherence | float | 0 to 2 | 1.000  |
| 11 | `grp_high` | High Group Coherence | float | 0 to 2 | 1.000  |
| 12 | `cycle_cents` | Cycle Detune (cents) | float | 0 to 50 | 6.000 ct |
| 13 | `cycle_speed` | Cycle Speed | float | 0 to 1 | 0.300  |
| 14 | `chaos_mode` | Chaos Mode | list | Locked|Drifting|Unstable | Drifting  |
| 15 | `chaos_type` | Chaos Type | list | Flicker|Wander|Jitter|Blend | Blend  |
| 16 | `chaos_amount` | Chaos Amount | float | 0 to 1 | 0.000  |
| 17 | `fold_amount` | Fold Amount | float | 0 to 1 | 0.000  |
| 18 | `fold_hz` | Fold Rate (Hz) | float | 1 to 60 | 17.000 Hz |
| 19 | `level` | Master Level | float | 0 to 1 | 0.600  |
| 20 | `freeze` | Freeze EM | toggle | Off / On | Off  |
| 21 | `drone` | Drone On | toggle | Off / On | On  |
| 22 | `follow_midi` | Follow MIDI Note | toggle | Off / On | On  |

Parameter IDs are the table indices above, then 23 + band for on, 40 + band for level, 57 + band for trim (band 0-16), 74 in total.
Meanings are in `Docs/CARRIER_FIELD.md` (Gravity, Phase Field Lock, Coherence, Cycle Detune, Chaos, Fold, Freeze EM, harmonic divisor).

## Known limitations
- Drone only; 17 bands are one voice. 17 Hz fold is phase modulation, not a wavefolder. No custom editor. No oversampling.
- Freeze restores as a parameter, not as a captured snapshot of the carrier field: live phases and drift restart from the engine's deterministic seed when the plugin is instantiated.
- Parameter changes inside a block are applied at their points, but the DSP smooths them; very fast automation is therefore low-passed by design.

## Verification status
- Tested here (g++, ASan/UBSan clean): `Tests/carrierfield_vst3_bridge_tests.cpp` covers normalized/plain round trips for all 74 parameters, text conversion, state format (round trip, truncation, bad magic/version/count, NaN, older state), sample-accurate event rendering, event overflow, and fuzz.
- Adapter logic (process queues, MIDI events, bus arrangements, state streams, controller strings) ran against a small stand-in for the SDK interfaces.
- NOT done: compiling against the real Steinberg SDK, running the validator, loading in any DAW. The SDK-specific code was written from knowledge of the SDK API; the `vst3-sdk` CI job is the first real compile. Likely first fixes: CMake helper names for your SDK version, `moduleinfo` handling, minor signature differences.
