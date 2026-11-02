#pragma once
/**
 * Composite Analog-inspired VCO.
 * Combines StableOscillator + NonlinearTuning + OscillatorDrift.
 * Clean mode is pure digital; character modes add controlled imperfections.
 */

#include "StableOscillator.h"
#include "../AnalogModel/NonlinearTuning.h"
#include "../AnalogModel/OscillatorDrift.h"

namespace cosmic::dsp {

class AnalogVCO {
public:
    AnalogVCO() = default;

    void setSampleRate(double sr) noexcept {
        osc_.setSampleRate(sr);
        drift_.setSampleRate(sr);
        sampleRate_ = sr;
    }

    void setFrequency(double freq) noexcept {
        baseFreq_ = freq;
        updateFrequency();
    }

    void setWaveform(Waveform w) noexcept { osc_.setWaveform(w); }
    void setPulseWidth(double pw) noexcept { osc_.setPulseWidth(pw); }

    NonlinearTuning& tuning() noexcept { return tuning_; }
    OscillatorDrift& drift() noexcept { return drift_; }
    StableOscillator& core() noexcept { return osc_; }

    void reset(double phase01 = 0.0) noexcept {
        osc_.reset(phase01);
        drift_.reset();
    }

    float process() noexcept {
        drift_.process();
        updateFrequency();
        float s = osc_.process();
        // Optional amplitude modulation from drift
        s *= static_cast<float>(1.0 + drift_.amplitudeOffset());
        return s;
    }

private:
    void updateFrequency() noexcept {
        double f = baseFreq_;
        f = tuning_.map(f, 0.0, 0.0); // controlV / amp can be driven externally later
        f += drift_.frequencyOffset();
        osc_.setFrequency(std::max(0.0, f));
    }

    StableOscillator osc_;
    NonlinearTuning tuning_;
    OscillatorDrift drift_;
    double baseFreq_ = 440.0;
    double sampleRate_ = 44100.0;
};

} // namespace cosmic::dsp
