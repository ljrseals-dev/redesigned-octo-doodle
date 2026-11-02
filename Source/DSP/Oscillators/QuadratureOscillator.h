#pragma once
/**
 * Shared-phase quadrature (I/Q) oscillator.
 * Guarantees exact 90° relationship in clean mode.
 * Useful for stereo FM, ring mod, frequency shifting.
 */

#include "PhaseAccumulator.h"
#include <cmath>
#include <utility>

namespace cosmic::dsp {

class QuadratureOscillator {
public:
    QuadratureOscillator() = default;

    void setSampleRate(double sr) noexcept { phase_.setSampleRate(sr); }
    void setFrequency(double freq) noexcept { phase_.setFrequency(freq); }

    void reset(double phase01 = 0.0) noexcept { phase_.reset(phase01); }

    // Returns (I, Q) = (cos, sin)
    std::pair<float, float> process() noexcept {
        const double p = phase_.tick();
        const double angle = p * PhaseAccumulator::kTwoPi;
        return { static_cast<float>(std::cos(angle)),
                 static_cast<float>(std::sin(angle)) };
    }

    // Convenience: process and write to two buffers
    void processBlock(float* iOut, float* qOut, size_t n) noexcept {
        for (size_t i = 0; i < n; ++i) {
            auto [ii, qq] = process();
            iOut[i] = ii;
            qOut[i] = qq;
        }
    }

    double getPhase01() const noexcept { return phase_.getPhase01(); }
    PhaseAccumulator& accumulator() noexcept { return phase_; }

private:
    PhaseAccumulator phase_;
};

} // namespace cosmic::dsp
