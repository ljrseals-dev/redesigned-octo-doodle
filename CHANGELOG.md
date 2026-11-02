# Changelog

## 0.1.0 — 2026-10-30

### Added
- Core 64-bit PhaseAccumulator
- StableOscillator (sine, triangle, saw, soft pulse)
- QuadratureOscillator with shared phase
- AnalogVCO composite (tuning + drift)
- NonlinearTuning & OscillatorDrift modules
- PLLCore + LoopFilter (musical phase tracking)
- FrequencyDivider (event & phase modes)
- Resonator (2nd-order, self-oscillation capable)
- Experimental SigmaDelta1
- Header-only CMake interface library
- Standalone unit tests
- Minimal JUCE plugin stub (VST3/Standalone when JUCE_PATH provided)
- Documentation & marketing package

### Known limitations
- No full PolyBLEP band-limiting yet
- PLL is a simplified prototype — not production RF-grade
- UI is GenericAudioProcessorEditor only
- No polyphonic voice management
