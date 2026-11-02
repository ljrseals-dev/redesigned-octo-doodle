# CarrierField (17-band carrier-field engine)

Header-only, allocation-free. `Source/DSP/CarrierField/{BandDrift.h, CarrierField.h}`.
Drone/texture only: there is no note, sequencing or arpeggiator logic.

## Signal path
17 band generators (64-bit PhaseAccumulator) -> per-band 17 Hz phase fold -> band mixer
(coherence-aware normalisation, tanh limiter) with a control-rate loop (about 1.5 kHz) applying
Gravity / Phase Field Lock / Coherence, and BandDrift (Cycle Detune + Chaos) feeding frequency,
phase and fold depth. Freeze EM crossfades to latched values over about 20 ms.

## Interpretations (decisions you can change)
| Control | Meaning here |
|---|---|
| Master Gravity (0..1) | Frequency pull toward the nearest harmonic of f0/harmonicDivisor (log-domain blend) plus phase relaxation toward the harmonic phase h*Phi_ref |
| Phase Field Lock (0..1) | Kuramoto-style mean-field pull toward each group's circular-mean phase (low 0-5, mid 6-11, high 12-16) |
| Harmony Coherence x1/x20/x100 | Loop-gain multiplier. Pulls are exact exponential relaxations per tick, so high gain is stable; frequency correction is slew-limited to 30% of band frequency |
| harmonicDivisor (default 4) | Gravity grid = partials of f0/D. D=1 collapses 17 bands onto only 8 harmonics (pulls up to 700 cents); D=4 gives 13 distinct harmonics, max pull 200 cents (12-TET) |
| 17 Hz fold | Per-band triangle modulator at foldHz adding up to +/-0.35 cycle of phase offset (foldAmount 0..1); depth also modulated by chaos |
| Freeze EM | Latches per-band frequency, fold depth, phase offset and level; drift keeps running underneath, so unfreezing is click-free |
| Cycle Detune | BandDrift (1/f + random walk + jitter) per band, depth in cents, speed 0.05x..4x |
| Chaos Mode/Amount/Type | Mode scales drift (x0.25 / x1 / x3) and loop gain (x1 / x1 / x0.2); Type picks Flicker / Wander / Jitter / Blend; Amount 0..1 |
| TET / octaves | tet 1..96 divisions; band step = i*stride mod (span*tet), default stride about a fifth; octaveShift clamped below Nyquist |

## Known limits
- Interpretations above are mine; the original ZS KMGTP behaviour is unverified.
- Output level is coherence-compensated but not loudness-matched: coherent states are pulse-like with lower RMS.
- Bands above 0.45*sr fade out. Folding is phase modulation, not a wavefolder; not oversampled.
- Drift presets are invented (inherited from analog-drift); tune by ear.
- Not yet wired into the JUCE PluginProcessor.
