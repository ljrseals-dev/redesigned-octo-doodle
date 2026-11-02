#pragma once
/**
 * Experimental first-order sigma-delta for fractional control / research.
 * Isolated module — do not insert into main audio path without measurement.
 */

#include <cmath>
#include <algorithm>

namespace cosmic::dsp {

class SigmaDelta1 {
public:
    SigmaDelta1() = default;

    void reset() noexcept {
        integrator_ = 0.0;
        lastQuant_ = 0.0;
    }

    // Input in [-1,1], returns quantized ±1 with noise shaping
    float process(float x) noexcept {
        const double err = static_cast<double>(x) - lastQuant_;
        integrator_ += err;
        // 1-bit quantize
        lastQuant_ = (integrator_ >= 0.0) ? 1.0 : -1.0;
        return static_cast<float>(lastQuant_);
    }

    double getIntegrator() const noexcept { return integrator_; }

private:
    double integrator_ = 0.0;
    double lastQuant_ = 0.0;
};

} // namespace cosmic::dsp
