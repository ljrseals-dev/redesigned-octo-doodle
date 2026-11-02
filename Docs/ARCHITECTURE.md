# Architecture

## Design goals
- Header-only, zero-dependency DSP core
- Real-time safe (no alloc, no locks, no I/O in process)
- Deterministic offline rendering
- Clean digital mode + optional character modes
- Modular: same components usable in instruments or effects

## Module map

```
PhaseAccumulator          ← foundation of all oscillators
    ↓
StableOscillator          ← waveforms
QuadratureOscillator      ← I/Q from shared phase
    ↓
AnalogVCO                 ← + NonlinearTuning + Drift
    ↓
PLLCore / FrequencyDivider / Resonator
```

## Real-time contract
See REALTIME_CONTRACT.md

## Extending
New modules should:
1. Be allocation-free after prepare/setSampleRate
2. Provide reset() for deterministic state
3. Accept sample-rate changes safely
4. Expose pure getters for testing
