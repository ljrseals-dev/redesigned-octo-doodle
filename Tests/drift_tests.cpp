#include "../Source/DSP/AnalogModel/OscillatorDrift.h"
#include <iostream>
#include <cmath>

using namespace cosmic::dsp;

int main() {
    int fail = 0;
    OscillatorDrift d1, d2;
    d1.setSampleRate(44100); d2.setSampleRate(44100);
    d1.setEnabled(true); d2.setEnabled(true);
    d1.setDepth(1.5); d2.setDepth(1.5);
    d1.setSeed(12345); d2.setSeed(12345);
    d1.reset(); d2.reset();
    for (int i = 0; i < 5000; ++i) {
        d1.process(); d2.process();
        if (std::abs(d1.frequencyOffset() - d2.frequencyOffset()) > 1e-12) ++fail;
    }
    // Bounds
    OscillatorDrift d3;
    d3.setSampleRate(44100); d3.setEnabled(true); d3.setDepth(2.0); d3.setSeed(7);
    for (int i = 0; i < 20000; ++i) {
        d3.process();
        if (std::abs(d3.frequencyOffset()) > 2.5) ++fail;
    }
    std::cout << "Drift tests: " << (fail ? "FAIL" : "PASS") << "\n";
    return fail ? 1 : 0;
}
