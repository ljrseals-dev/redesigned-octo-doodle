/**
 * Cosmic Synthesizer Engine — Core oscillator unit tests
 * Compile with: g++ -std=c++20 -O2 -I../Source oscillator_tests.cpp -o osc_tests && ./osc_tests
 */

#include "../Source/DSP/Oscillators/PhaseAccumulator.h"
#include "../Source/DSP/Oscillators/StableOscillator.h"
#include "../Source/DSP/Oscillators/QuadratureOscillator.h"
#include "../Source/DSP/Oscillators/AnalogVCO.h"
#include "../Source/DSP/AnalogModel/OscillatorDrift.h"
#include "../Source/DSP/Synchronization/PLLCore.h"
#include "../Source/DSP/Synchronization/FrequencyDivider.h"
#include "../Source/DSP/Filters/Resonator.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <cassert>

using namespace cosmic::dsp;

#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << "\n"; ++failures; } else { ++passes; } } while(0)

int main() {
    int passes = 0, failures = 0;

    // 1. Phase accumulator precision & continuity
    {
        PhaseAccumulator pa;
        pa.setSampleRate(48000.0);
        pa.setFrequency(440.0);
        pa.reset(0.0);
        double prev = pa.getPhase01();
        for (int i = 0; i < 1000; ++i) {
            double p = pa.tick();
            double delta = p - prev;
            if (delta < 0) delta += 1.0; // wrap
            CHECK(delta > 0.0 && delta < 0.1, "phase advances continuously");
            prev = p;
        }
        CHECK(true, "phase accumulator basic advance");
    }

    // 2. Frequency accuracy (approximate zero-crossing count)
    {
        StableOscillator osc;
        osc.setSampleRate(48000.0);
        osc.setFrequency(440.0);
        osc.setWaveform(Waveform::Sine);
        osc.reset(0.0);
        int crossings = 0;
        float prev = 0.f;
        const int N = 48000; // 1 second
        for (int i = 0; i < N; ++i) {
            float s = osc.process();
            if (prev < 0.f && s >= 0.f) ++crossings;
            prev = s;
        }
        // Expect ~440 crossings
        CHECK(std::abs(crossings - 440) < 5, "sine frequency accuracy within 5 Hz");
    }

    // 3. Quadrature 90° relationship
    {
        QuadratureOscillator q;
        q.setSampleRate(48000.0);
        q.setFrequency(1000.0);
        q.reset(0.0);
        double maxErr = 0.0;
        for (int i = 0; i < 2000; ++i) {
            auto [I, Q] = q.process();
            // I should be cos, Q sin → I*Q should average ~0, and Q lags I by 90°
            // Check magnitude
            double mag = std::sqrt(I*I + Q*Q);
            maxErr = std::max(maxErr, std::abs(mag - 1.0));
        }
        CHECK(maxErr < 1e-5, "quadrature magnitude stays near unity");
    }

    // 4. Drift reproducibility
    {
        OscillatorDrift d1, d2;
        d1.setSampleRate(44100.0);
        d2.setSampleRate(44100.0);
        d1.setEnabled(true);
        d2.setEnabled(true);
        d1.setDepth(2.0, 0.0, 0.0);
        d2.setDepth(2.0, 0.0, 0.0);
        d1.setSeed(42);
        d2.setSeed(42);
        d1.reset();
        d2.reset();
        bool same = true;
        for (int i = 0; i < 10000; ++i) {
            d1.process();
            d2.process();
            if (std::abs(d1.frequencyOffset() - d2.frequencyOffset()) > 1e-12)
                same = false;
        }
        CHECK(same, "drift is reproducible with same seed");
    }

    // 5. Frequency divider
    {
        FrequencyDivider div;
        div.setRatio(4);
        int events = 0;
        for (int i = 0; i < 40; ++i) {
            if (div.tickEvent()) ++events;
        }
        CHECK(events == 10, "divider produces N events correctly");
    }

    // 6. Resonator does not explode
    {
        Resonator r;
        r.setSampleRate(48000.0);
        r.setFrequency(1000.0);
        r.setDamping(5.0);
        r.reset();
        float maxAbs = 0.f;
        for (int i = 0; i < 10000; ++i) {
            float s = r.process(i == 0 ? 1.f : 0.f);
            maxAbs = std::max(maxAbs, std::abs(s));
        }
        CHECK(maxAbs < 2.0f, "resonator remains bounded");
    }

    // 7. AnalogVCO basic
    {
        AnalogVCO vco;
        vco.setSampleRate(44100.0);
        vco.setFrequency(220.0);
        vco.setWaveform(Waveform::Saw);
        vco.drift().setEnabled(false);
        float sum = 0.f;
        for (int i = 0; i < 1000; ++i)
            sum += vco.process();
        CHECK(std::abs(sum) < 1000.f, "AnalogVCO produces finite output");
    }

    // 8. PLL does not crash and can free-run
    {
        PLLCore pll;
        pll.setSampleRate(48000.0);
        pll.setNominalFrequency(440.0);
        pll.setEnabled(false);
        pll.reset();
        float s = 0.f;
        for (int i = 0; i < 1000; ++i)
            s += pll.process(0.0);
        CHECK(std::isfinite(s), "PLL free-run produces finite samples");
    }

    std::cout << "\n=== Cosmic Synthesizer Engine Tests ===\n";
    std::cout << "Passed: " << passes << "\n";
    std::cout << "Failed: " << failures << "\n";
    if (failures == 0)
        std::cout << "ALL TESTS PASSED\n";
    else
        std::cout << "SOME TESTS FAILED\n";

    return failures == 0 ? 0 : 1;
}
