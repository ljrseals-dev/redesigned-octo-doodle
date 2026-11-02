#pragma once
/**
 * Stable band-limited-aware oscillator core.
 * 64-bit phase accumulator + PolyBLEP residual for saw & pulse.
 * Sine / triangle remain analytic. Phase continuous, deterministic reset.
 */

#include "PhaseAccumulator.h"
#include <cmath>
#include <algorithm>

namespace cosmic::dsp {

enum class Waveform {
    Sine = 0,
    Triangle,
    Saw,
    Pulse,
    Count
};

class StableOscillator {
public:
    StableOscillator() = default;

    void setSampleRate(double sr) noexcept {
        phase_.setSampleRate(sr);
        sampleRate_ = std::max(1.0, sr);
        updatePhaseInc();
    }

    void setFrequency(double freq) noexcept {
        frequency_ = std::clamp(freq, 0.0, sampleRate_ * 0.499);
        phase_.setFrequency(frequency_);
        updatePhaseInc();
    }

    void setWaveform(Waveform w) noexcept { waveform_ = w; }
    void setPulseWidth(double pw) noexcept { pulseWidth_ = std::clamp(pw, 0.01, 0.99); }

    void reset(double phase01 = 0.0) noexcept {
        phase_.reset(phase01);
    }

    float process() noexcept {
        const double t = phase_.tick();          // [0, 1)
        const double dt = phaseInc_;             // phase increment in [0, 0.5)

        switch (waveform_) {
            case Waveform::Sine:
                return static_cast<float>(std::sin(t * PhaseAccumulator::kTwoPi));

            case Waveform::Triangle: {
                double tri = 1.0 - 4.0 * std::abs(t - 0.5);
                return static_cast<float>(tri);
            }

            case Waveform::Saw: {
                double saw = 2.0 * t - 1.0;
                saw -= polyBLEP(t, dt);
                return static_cast<float>(saw);
            }

            case Waveform::Pulse: {
                double pulse = (t < pulseWidth_) ? 1.0 : -1.0;
                pulse += polyBLEP(t, dt);
                pulse -= polyBLEP(std::fmod(t + (1.0 - pulseWidth_), 1.0), dt);
                return static_cast<float>(pulse);
            }

            default:
                return 0.f;
        }
    }

    double getPhase01() const noexcept { return phase_.getPhase01(); }
    double getFrequency() const noexcept { return frequency_; }
    Waveform getWaveform() const noexcept { return waveform_; }

    PhaseAccumulator& accumulator() noexcept { return phase_; }
    const PhaseAccumulator& accumulator() const noexcept { return phase_; }

private:
    void updatePhaseInc() noexcept {
        phaseInc_ = frequency_ / sampleRate_;
    }

    static double polyBLEP(double t, double dt) noexcept {
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt) {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    PhaseAccumulator phase_;
    Waveform waveform_ = Waveform::Sine;
    double pulseWidth_ = 0.5;
    double sampleRate_ = 44100.0;
    double frequency_  = 440.0;
    double phaseInc_   = 0.0;
};

} // namespace cosmic::dsp
