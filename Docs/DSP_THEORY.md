# DSP Theory Notes (Audio Adaptation)

## Phase accumulation
A 64-bit phase register advanced by `freq/sr * 2^64` each sample gives sub-micro-Hz resolution at audio rates and exact wrap-around.

## Quadrature
Shared accumulator + cos/sin evaluation guarantees a mathematical 90° relationship, independent of two free-running oscillators.

## Nonlinear tuning
Polynomial + amplitude coupling terms allow creative pitch curves. Coefficients are free parameters, not physical measurements.

## Drift
Seeded LCG + one-pole smoother produces slowly varying, bounded, reproducible offsets. Disabled by default.

## PLL-style tracker
Phase error → PI filter → frequency correction. Suitable for soft-sync and musical locking experiments. Not a drop-in replacement for RF PLL silicon.

## Frequency division
Separate event-counter and continuous-phase modes. Useful for subharmonics and clock subdivision.

## Resonator
Classic second-order recursive form with controllable radius (damping). Can be driven into self-oscillation.
