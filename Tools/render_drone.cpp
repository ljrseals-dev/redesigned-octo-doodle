// Offline drone renderer for the CarrierField engine. Writes a 16-bit mono WAV and a CSV of
// coherence over time. No notes, no sequencing: one continuous texture driven by automation.
// Usage: render_drone [out.wav] [out.csv] [sampleRate]
#include "../Source/DSP/CarrierField/CarrierField.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

using namespace cosmic::dsp;

static void writeWav(const char* path, const std::vector<int16_t>& d, int sr) {
    FILE* f = std::fopen(path, "wb"); if (!f) { std::perror("wav"); std::exit(1); }
    uint32_t dataBytes = static_cast<uint32_t>(d.size() * 2), riff = 36 + dataBytes, fmt = 16, rate = sr, brate = sr * 2;
    uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt, 4, 1, f); std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f);
    std::fwrite(&rate, 4, 1, f); std::fwrite(&brate, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dataBytes, 4, 1, f);
    std::fwrite(d.data(), 2, d.size(), f); std::fclose(f);
}

static double ramp(double t, double a, double b, double v0, double v1) {
    if (t <= a) return v0;
    if (t >= b) return v1;
    return v0 + (v1 - v0) * (t - a) / (b - a);
}

int main(int argc, char** argv) {
    const char* wavPath = argc > 1 ? argv[1] : "carrier_field_demo.wav";
    const char* csvPath = argc > 2 ? argv[2] : "carrier_field_demo.csv";
    const int sr = argc > 3 ? std::atoi(argv[3]) : 48000;
    const double total = 56.0;

    CarrierField cf; cf.setSampleRate(sr);
    CarrierField::Params p; p.fundamentalHz = 55.0; p.tet = 12; p.seed = 7;
    p.gravity = 0; p.phaseFieldLock = 0; p.chaosAmount = 0; p.cycleDetuneCents = 8; p.foldAmount = 0.25;
    cf.setParams(p); cf.reset();

    std::vector<int16_t> pcm; pcm.reserve(static_cast<size_t>(total * sr));
    FILE* csv = std::fopen(csvPath, "w"); std::fprintf(csv, "t,coherence,gravity,chaos,freeze\n");
    const int block = 480;
    for (int n = 0; n < static_cast<int>(total * sr); ++n) {
        const double t = static_cast<double>(n) / sr;
        if (n % block == 0) {
            auto q = cf.params();
            q.gravity       = ramp(t, 5, 22, 0.0, 1.0);          // free carriers -> harmonic body
            q.coherenceMult = t < 24 ? 1 : (t < 30 ? 20 : 100);  // x1 -> x20 -> x100
            q.phaseFieldLock= ramp(t, 24, 28, 0.0, 1.0);
            q.mode          = t < 38 ? CarrierField::ChaosMode::Drifting : CarrierField::ChaosMode::Unstable;
            q.chaosAmount   = ramp(t, 32, 46, 0.0, 0.9);          // stable -> drifting
            q.foldAmount    = ramp(t, 14, 30, 0.1, 0.6);
            q.freeze        = (t >= 46 && t < 52);               // Freeze EM while chaos is at peak
            q.octaveShift   = t < 20 ? 0 : (t < 26 ? 1 : 0);     // octave shift mid-run
            cf.setParams(q);
            std::fprintf(csv, "%.3f,%.4f,%.3f,%.3f,%d\n", t, cf.coherence(), q.gravity, q.chaosAmount, q.freeze ? 1 : 0);
        }
        float x = cf.process();
        pcm.push_back(static_cast<int16_t>(std::lrint(std::clamp(x, -1.0f, 1.0f) * 32767.0f)));
    }
    std::fclose(csv); writeWav(wavPath, pcm, sr);
    std::printf("wrote %s (%zu samples @ %d Hz) and %s\n", wavPath, pcm.size(), sr, csvPath);
    return 0;
}
