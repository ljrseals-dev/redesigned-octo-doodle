#pragma once
/**
 * Cosmic Synthesizer Engine — 64-bit Phase Accumulator
 * Precision at the Core. Character at the Edges.
 *
 * Provides deterministic, phase-continuous frequency control.
 * Inspired by high-resolution digital control concepts; original audio DSP.
 */

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace cosmic::dsp {

class PhaseAccumulator {
public:
    using PhaseT = uint64_t;

    static constexpr PhaseT kFullCycle = ~PhaseT(0); // 2^64 - 1
    static constexpr double kTwoPi = 6.283185307179586476925286766559;

    PhaseAccumulator() = default;

    void setSampleRate(double sr) noexcept {
        sampleRate_ = std::max(1.0, sr);
        updateIncrement();
    }

    void setFrequency(double freqHz) noexcept {
        frequency_ = std::clamp(freqHz, 0.0, sampleRate_ * 0.499);
        updateIncrement();
    }

    double getFrequency() const noexcept { return frequency_; }
    double getSampleRate() const noexcept { return sampleRate_; }

    // Advance one sample; returns phase in [0, 1)
    double tick() noexcept {
        phase_ += increment_;
        // Natural wrap at 2^64
        return static_cast<double>(phase_) * (1.0 / 18446744073709551616.0);
    }

    // Advance N samples without generating output (for efficiency)
    void advance(size_t n) noexcept {
        phase_ += increment_ * static_cast<PhaseT>(n);
    }

    void reset(double phase01 = 0.0) noexcept {
        phase_ = static_cast<PhaseT>(std::fmod(std::max(0.0, phase01), 1.0) * 18446744073709551616.0);
    }

    double getPhase01() const noexcept {
        return static_cast<double>(phase_) * (1.0 / 18446744073709551616.0);
    }

    PhaseT getRawPhase() const noexcept { return phase_; }

    // Force phase to a specific raw value (for deterministic tests)
    void setRawPhase(PhaseT p) noexcept { phase_ = p; }

private:
    void updateIncrement() noexcept {
        // phaseIncrement = freq / sr * 2^64
        double ratio = frequency_ / sampleRate_;
        increment_ = static_cast<PhaseT>(ratio * 18446744073709551616.0);
    }

    double sampleRate_ = 44100.0;
    double frequency_  = 440.0;
    PhaseT phase_      = 0;
    PhaseT increment_  = 0;
};

} // namespace cosmic::dsp
