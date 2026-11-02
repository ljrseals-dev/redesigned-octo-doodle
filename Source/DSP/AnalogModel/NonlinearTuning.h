#pragma once
/**
 * Configurable nonlinear frequency mapping.
 * f_actual = f_nominal + a1*v + a2*v^2 + a3*v^3 + kA*(A - A0)
 * Allows creative analog-style pitch curves without claiming physical accuracy.
 */

#include <cmath>
#include <algorithm>

namespace cosmic::dsp {

class NonlinearTuning {
public:
    NonlinearTuning() = default;

    // Control voltage / normalized frequency parameter in [-1, 1] or [0, 1]
    void setCoefficients(double a1, double a2, double a3) noexcept {
        a1_ = a1; a2_ = a2; a3_ = a3;
    }

    void setAmplitudeCoupling(double kA, double A0 = 0.0) noexcept {
        kA_ = kA; A0_ = A0;
    }

    void setEnabled(bool e) noexcept { enabled_ = e; }
    bool isEnabled() const noexcept { return enabled_; }

    // Apply correction: returns corrected frequency in Hz
    double map(double nominalHz, double controlV = 0.0, double amplitude = 0.0) const noexcept {
        if (!enabled_)
            return nominalHz;

        const double v = controlV;
        const double poly = a1_ * v + a2_ * v * v + a3_ * v * v * v;
        const double ampTerm = kA_ * (amplitude - A0_);
        return std::max(0.0, nominalHz + poly + ampTerm);
    }

private:
    bool enabled_ = false;
    double a1_ = 0.0, a2_ = 0.0, a3_ = 0.0;
    double kA_ = 0.0, A0_ = 0.0;
};

} // namespace cosmic::dsp
