# Cosmic Synthesizer Engine 0.1.0

**Precision at the Core. Character at the Edges.**

A reusable C++20 DSP foundation for advanced analog-inspired VST synthesizers.

## Engineering Inspiration

This project translates selected *engineering principles* from the field of CMOS RF and mm-Wave integrated circuit design — particularly oscillator stability, quadrature generation, phase-locked loops, frequency division and controlled nonlinear behaviour — into original real-time audio DSP.

Primary reference (inspiration only, no text reproduced):

> *CMOS RF and mm-Wave Transceivers and Synthesizers*  
> Bharatha Kumar Thangarasu, Nagarajan Mahalingam, Kaixue Ma, Kiat Seng Yeo  
> Jenny Stanford Publishing

The book concerns RF integrated circuits. This repository is an independent audio software project. Only abstract principles (phase accumulation, feedback control, noise shaping, deterministic drift) are adapted.

## Features (0.1.0)

### Implemented
- 64-bit phase accumulator with continuous frequency changes
- Stable sine / triangle / PolyBLEP saw & pulse oscillators
- Shared-phase quadrature (I/Q) oscillator
- Configurable nonlinear tuning curves + amplitude-to-frequency coupling
- Deterministic seeded oscillator drift (frequency / phase / amplitude)
- Basic PLL-style phase tracker with PI loop filter and lock detection
- Integer frequency / event divider
- Second-order resonator with adjustable damping
- Experimental first-order sigma-delta module (research only)
- Header-only C++20 DSP library
- Standalone unit tests (no JUCE required)
- Full JUCE VST3 / Standalone plugin source (activated when `JUCE_PATH` is supplied)
- Cosmic dark UI with violet / magenta / cyan accents

### Not yet production-ready
- Full multi-voice polyphony & voice management
- Advanced fractional-N / sigma-delta in the audio path
- Complete measured spectral anti-aliasing validation suite

## Build

```bash
# DSP + tests only (no JUCE needed)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCOSMIC_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure

# With JUCE (developer must supply the SDK)
cmake -S . -B build -DJUCE_PATH=/path/to/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Real-time contract

All DSP modules are allocation-free, lock-free and free of I/O inside the processing path.  
Sample-rate and block-size changes are handled by explicit `setSampleRate` / `reset` calls.

## License

MIT — see LICENSE

## Authors

Cosmic DSP research implementation, 2026.
