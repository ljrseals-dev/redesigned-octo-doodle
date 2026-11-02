#include "../Source/DSP/Synchronization/FrequencyDivider.h"
#include <iostream>

using namespace cosmic::dsp;

int main() {
    int fail = 0;
    FrequencyDivider div;
    div.setRatio(3);
    int events = 0;
    for (int i = 0; i < 30; ++i)
        if (div.tickEvent()) ++events;
    if (events != 10) ++fail;
    std::cout << "Divider tests: " << (fail ? "FAIL" : "PASS") << "\n";
    return fail;
}
