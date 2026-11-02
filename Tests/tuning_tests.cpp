#include "../Source/DSP/AnalogModel/NonlinearTuning.h"
#include <iostream>
#include <cmath>

using namespace cosmic::dsp;

int main() {
    int fail = 0;
    NonlinearTuning t;
    t.setEnabled(true);
    t.setCoefficients(2.0, 0.5, 0.1);
    double f = t.map(440.0, 0.5, 0.0);
    if (f <= 440.0) { std::cerr << "expected upward correction\n"; ++fail; }
    t.setEnabled(false);
    if (std::abs(t.map(440.0, 0.5) - 440.0) > 1e-9) { ++fail; }
    std::cout << "Tuning tests: " << (fail ? "FAIL" : "PASS") << "\n";
    return fail;
}
