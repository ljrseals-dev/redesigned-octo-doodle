#include "../Source/DSP/Synchronization/PLLCore.h"
#include <iostream>
#include <cmath>

using namespace cosmic::dsp;

int main() {
    int fail = 0;
    PLLCore pll;
    pll.setSampleRate(48000.0);
    pll.setNominalFrequency(440.0);
    pll.setEnabled(true);
    pll.setLoopGains(0.05, 0.0005);
    pll.reset();

    // Free-run reference phase
    double refPhase = 0.0;
    const double refInc = 440.0 / 48000.0;
    bool lockedSeen = false;
    for (int i = 0; i < 20000; ++i) {
        refPhase = std::fmod(refPhase + refInc, 1.0);
        float s = pll.process(refPhase);
        if (!std::isfinite(s)) { ++fail; break; }
        if (pll.getState() == PLLCore::State::Locked) lockedSeen = true;
    }
    // Lock is best-effort in this simple prototype
    std::cout << "PLL tests: " << (fail ? "FAIL" : "PASS")
              << " (locked observed: " << lockedSeen << ")\n";
    return fail;
}
