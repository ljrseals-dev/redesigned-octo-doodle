#pragma once
/**
 * Integer event / cycle divider.
 * Separate from continuous frequency scaling.
 * Sample-accurate for trigger/clock use.
 */

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace cosmic::dsp {

class FrequencyDivider {
public:
    FrequencyDivider() = default;

    void setRatio(int n) noexcept {
        ratio_ = std::max(1, n);
        counter_ = 0;
    }

    int getRatio() const noexcept { return ratio_; }

    void reset() noexcept { counter_ = 0; output_ = false; }

    // Call on each rising edge / cycle of the input
    // Returns true on every N-th event
    bool tickEvent() noexcept {
        ++counter_;
        if (counter_ >= ratio_) {
            counter_ = 0;
            output_ = !output_; // toggle for square-like sub
            return true;
        }
        return false;
    }

    // Continuous phase-based division (for audio-rate)
    // inputPhase01 advances; returns divided phase [0,1)
    double processPhase(double inputPhase01) noexcept {
        // Simple fractional accumulate
        double delta = inputPhase01 - lastInputPhase_;
        if (delta < -0.5) delta += 1.0; // unwrap
        if (delta > 0.5) delta -= 1.0;
        lastInputPhase_ = inputPhase01;

        phase_ += delta / static_cast<double>(ratio_);
        phase_ = phase_ - std::floor(phase_);
        return phase_;
    }

    bool getOutputLevel() const noexcept { return output_; }
    double getPhase01() const noexcept { return phase_; }

private:
    int ratio_ = 2;
    int counter_ = 0;
    bool output_ = false;
    double phase_ = 0.0;
    double lastInputPhase_ = 0.0;
};

} // namespace cosmic::dsp
