#pragma once
/**
 * Basic PLL-style phase tracker for musical synchronization.
 * Phase detector + PI loop filter + frequency correction.
 * Not a production RF PLL; intended for oscillator locking & soft sync.
 */

#include "../Oscillators/PhaseAccumulator.h"
#include "LoopFilter.h"
#include <cmath>
#include <algorithm>

namespace cosmic::dsp {

class PLLCore {
public:
    enum class State { FreeRunning, Acquiring, Locked, LostReference };

    PLLCore() = default;

    void setSampleRate(double sr) noexcept {
        sampleRate_ = std::max(1.0, sr);
        slave_.setSampleRate(sr);
    }

    void setNominalFrequency(double f) noexcept {
        nominalFreq_ = f;
        slave_.setFrequency(f);
    }

    void setLoopGains(double kp, double ki) noexcept {
        filter_.setGains(kp, ki);
    }

    void setLockThreshold(double rad) noexcept { lockThreshold_ = rad; }
    void setEnabled(bool e) noexcept { enabled_ = e; }

    void reset() noexcept {
        filter_.reset();
        slave_.reset();
        state_ = State::FreeRunning;
        consecutiveLock_ = 0;
    }

    // Feed reference phase [0,1) each sample
    // Returns slave output sample (sine for simplicity)
    float process(double refPhase01) noexcept {
        if (!enabled_) {
            state_ = State::FreeRunning;
            slave_.setFrequency(nominalFreq_);
            return static_cast<float>(std::sin(slave_.tick() * PhaseAccumulator::kTwoPi));
        }

        const double slavePhase = slave_.getPhase01();
        double err = refPhase01 - slavePhase;
        // Wrap to [-0.5, 0.5]
        if (err > 0.5) err -= 1.0;
        if (err < -0.5) err += 1.0;
        const double errRad = err * PhaseAccumulator::kTwoPi;

        const double correction = filter_.process(errRad);
        const double newFreq = std::clamp(nominalFreq_ + correction, 0.0, sampleRate_ * 0.45);
        slave_.setFrequency(newFreq);

        // Advance slave
        const float out = static_cast<float>(std::sin(slave_.tick() * PhaseAccumulator::kTwoPi));

        // Simple lock detection
        if (std::abs(errRad) < lockThreshold_) {
            ++consecutiveLock_;
            if (consecutiveLock_ > 64)
                state_ = State::Locked;
            else
                state_ = State::Acquiring;
        } else {
            consecutiveLock_ = 0;
            state_ = State::Acquiring;
        }

        return out;
    }

    State getState() const noexcept { return state_; }
    double getSlaveFrequency() const noexcept { return slave_.getFrequency(); }
    PhaseAccumulator& slaveAccumulator() noexcept { return slave_; }

private:
    PhaseAccumulator slave_;
    LoopFilter filter_;
    double sampleRate_ = 44100.0;
    double nominalFreq_ = 440.0;
    double lockThreshold_ = 0.1; // rad
    bool enabled_ = false;
    State state_ = State::FreeRunning;
    int consecutiveLock_ = 0;
};

} // namespace cosmic::dsp
