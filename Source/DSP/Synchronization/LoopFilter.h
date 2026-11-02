#pragma once
/**
 * Simple PI loop filter for PLL-style tracking.
 * Bounded integrator, configurable gains.
 */

#include <algorithm>
#include <cmath>

namespace cosmic::dsp {

class LoopFilter {
public:
    LoopFilter() = default;

    void setGains(double kp, double ki) noexcept {
        kp_ = kp; ki_ = ki;
    }

    void setIntegratorLimit(double lim) noexcept {
        limit_ = std::abs(lim);
    }

    void reset() noexcept {
        integrator_ = 0.0;
        lastOutput_ = 0.0;
    }

    // Input: phase error in radians (wrapped preferred)
    // Output: frequency correction (Hz or normalized increment)
    double process(double phaseError) noexcept {
        integrator_ += phaseError * ki_;
        integrator_ = std::clamp(integrator_, -limit_, limit_);
        lastOutput_ = kp_ * phaseError + integrator_;
        return lastOutput_;
    }

    double getIntegrator() const noexcept { return integrator_; }
    double getLastOutput() const noexcept { return lastOutput_; }

private:
    double kp_ = 0.1;
    double ki_ = 0.001;
    double limit_ = 100.0; // Hz
    double integrator_ = 0.0;
    double lastOutput_ = 0.0;
};

} // namespace cosmic::dsp
