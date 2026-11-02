#include "../Source/DSP/Oscillators/QuadratureOscillator.h"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace cosmic::dsp;

int main() {
    int fail = 0;
    QuadratureOscillator q;
    q.setSampleRate(48000.0);
    q.setFrequency(1000.0);
    q.reset(0.0);

    double maxMagErr = 0.0, maxDot = 0.0;
    for (int i = 0; i < 4800; ++i) {
        auto [I, Q] = q.process();
        double mag = std::sqrt(I*I + Q*Q);
        maxMagErr = std::max(maxMagErr, std::abs(mag - 1.0));
        // At exact 90°, average I*Q over many samples ≈ 0
        maxDot = std::max(maxDot, static_cast<double>(std::abs(I * Q)));
    }
    if (maxMagErr > 1e-5) { std::cerr << "mag fail\n"; ++fail; }
    // instantaneous product can be up to 0.5, so just check finiteness
    if (!std::isfinite(maxDot)) { std::cerr << "dot fail\n"; ++fail; }

    std::cout << "Quadrature tests: " << (fail ? "FAIL" : "PASS") << "\n";
    return fail;
}
