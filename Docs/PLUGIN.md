# CarrierField plugin

Separate JUCE target `CarrierField` (VST3 + Standalone, synth, MIDI in, stereo out), next to the original `CosmicSynth`.
Enable by passing `-DJUCE_PATH=/path/to/JUCE` to CMake, then build `CarrierField_VST3` / `CarrierField_Standalone`.

## Structure
- `Source/Plugin/CarrierFieldInstrument.h` is JUCE-free: the single parameter table (74 parameters), `Controls` (host values -> engine settings), and `Instrument` (engine wrapper). Unit-tested without JUCE.
- `Source/Plugin/CarrierFieldProcessor.{h,cpp}` is the thin JUCE shell: builds the APVTS layout from the table, reads parameters once per block, forwards MIDI note-ons, renders mono to both channels, saves/loads state as XML.
- Editor: JUCE's `GenericAudioProcessorEditor` (all parameters, host-automatable). A custom UI is not built yet.

## Behaviour
- Always a drone. `Drone On` fades it in and out (10 ms). There is no note-off logic, no gate, no sequencing.
- A MIDI note-on sets the fundamental (note -> Hz) when `Follow MIDI Note` is on; the `Fundamental` parameter is used otherwise. All Notes Off clears the note.
- Parameters: tuning (fundamental, octave, TET, band step, span, harmonic divisor), field (gravity, phase field lock, coherence x1/x20/x100, three group coherences), cycle detune (cents, speed), chaos (mode, type, amount), fold (amount, rate), master level, freeze, and per-band on / level / trim for all 17 bands.
- Seed is fixed per instance, so a given state renders identically every time.
- Output is mono duplicated to both channels (no stereo spread yet).

## Verification status
- Parameter table, defaults, clamping, NaN handling, MIDI mapping, drone on/off, block-size independence (1 to 1000 samples), min/max/random-jump fuzz at 22.05-192 kHz: `Tests/carrierfield_plugin_tests.cpp` (also ASan/UBSan clean).
- The JUCE shell compiles and runs against a small stand-in for the JUCE API I used (wiring only). It has NOT been compiled against real JUCE here: the `juce-plugin` CI job does that (set `continue-on-error` until first green).
- Not yet done: loading in a DAW, listening tests, custom editor.
