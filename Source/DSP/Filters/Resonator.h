#pragma once
/**
 * Second-order digital resonator with adjustable damping / feedback.
 * Can self-oscillate for analog-style resonance experiments.
 */

#include <cmath>
#include <algorithm>

namespace cosmic::dsp {

class Resonator {
public:
    Resonator() = default;

    void setSampleRate(double sr) noexcept {
        sampleRate_ = std::max(1.0, sr);
        updateCoeffs();
    }

    void setFrequency(double freqHz) noexcept {
        frequency_ = std::clamp(freqHz, 1.0, sampleRate_ * 0.45);
        updateCoeffs();
    }

    // Damping: 0 = infinite Q / self-osc, higher = more damped
    void setDamping(double d) noexcept {
        damping_ = std::max(0.0, d);
        updateCoeffs();
    }

    void reset() noexcept {
        y1_ = y2_ = 0.0;
    }

    float process(float x) noexcept {
        // y[n] = 2*r*cos(theta)*y[n-1] - r^2 * y[n-2] + x
        const double y = a1_ * y1_ + a2_ * y2_ + static_cast<double>(x);
        y2_ = y1_;
        y1_ = y;
        // Soft clip to prevent runaway
        return static_cast<float>(std::tanh(y * 0.8));
    }

    double getFrequency() const noexcept { return frequency_; }

private:
    void updateCoeffs() noexcept {
        const double theta = 2.0 * 3.141592653589793 * frequency_ / sampleRate_;
        const double r = std::exp(-damping_ / sampleRate_); // simple decay mapping
        a1_ = 2.0 * r * std::cos(theta);
        a2_ = -r * r;
    }

    double sampleRate_ = 44100.0;
    double frequency_ = 1000.0;
    double damping_ = 10.0;
    double a1_ = 0.0, a2_ = 0.0;
    double y1_ = 0.0, y2_ = 0.0;
};

} // namespace cosmic::dsp
