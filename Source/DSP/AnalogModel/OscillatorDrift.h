#pragma once
/**
 * Deterministic, seeded, slowly-varying bounded frequency / phase / amplitude drift.
 * Disabled by default. Reproducible for offline rendering.
 */

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace cosmic::dsp {

class OscillatorDrift {
public:
    OscillatorDrift() = default;

    void setSampleRate(double sr) noexcept { sampleRate_ = std::max(1.0, sr); }
    void setEnabled(bool e) noexcept { enabled_ = e; }
    bool isEnabled() const noexcept { return enabled_; }

    // Depth in Hz (frequency), radians (phase), linear (amplitude)
    void setDepth(double freqHz, double phaseRad = 0.0, double amp = 0.0) noexcept {
        depthFreq_  = freqHz;
        depthPhase_ = phaseRad;
        depthAmp_   = amp;
    }

    // Seed for reproducibility
    void setSeed(uint32_t s) noexcept {
        seed_ = s;
        state_ = s ? s : 1u;
    }

    // Call once per sample (or block-rate for efficiency)
    void process() noexcept {
        if (!enabled_) {
            currentFreqOffset_ = 0.0;
            currentPhaseOffset_ = 0.0;
            currentAmpOffset_ = 0.0;
            return;
        }

        // Simple LCG + one-pole smoothing for slow drift
        state_ = state_ * 1664525u + 1013904223u;
        const double noise = (static_cast<double>(state_ & 0xFFFFFF) / 16777216.0) * 2.0 - 1.0;

        // Very slow update (approx 0.1–2 Hz depending on sample rate)
        const double alpha = 1.0 - std::exp(-2.0 * 3.141592653589793 * 0.3 / sampleRate_);
        smoothedNoise_ += alpha * (noise - smoothedNoise_);

        currentFreqOffset_  = smoothedNoise_ * depthFreq_;
        currentPhaseOffset_ = smoothedNoise_ * depthPhase_;
        currentAmpOffset_   = smoothedNoise_ * depthAmp_;
    }

    double frequencyOffset() const noexcept { return currentFreqOffset_; }
    double phaseOffset() const noexcept { return currentPhaseOffset_; }
    double amplitudeOffset() const noexcept { return currentAmpOffset_; }

    void reset() noexcept {
        smoothedNoise_ = 0.0;
        currentFreqOffset_ = currentPhaseOffset_ = currentAmpOffset_ = 0.0;
        state_ = seed_ ? seed_ : 1u;
    }

private:
    bool enabled_ = false;
    double sampleRate_ = 44100.0;
    double depthFreq_ = 0.0, depthPhase_ = 0.0, depthAmp_ = 0.0;
    uint32_t seed_ = 1;
    uint32_t state_ = 1;
    double smoothedNoise_ = 0.0;
    double currentFreqOffset_ = 0.0;
    double currentPhaseOffset_ = 0.0;
    double currentAmpOffset_ = 0.0;
};

} // namespace cosmic::dsp
