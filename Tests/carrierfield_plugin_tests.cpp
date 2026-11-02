#include "../Source/Plugin/CarrierFieldInstrument.h"
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <random>

using namespace cosmic::plugin;
static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::cout << "FAIL: " << msg << "\n"; } } while (0)
#define INFO(msg) std::cout << msg << "\n"

static int countChoices(const char* s) { int n = 1; for (; *s; ++s) if (*s == '|') ++n; return n; }

int main() {
    // 1. Parameter table integrity.
    {
        std::set<std::string> ids;
        for (int i = 0; i < kParamCount; ++i) {
            const auto& s = spec(i);
            CHECK(s.id && s.id[0], "param " << i << " has no id");
            CHECK(ids.insert(s.id).second, "duplicate id " << s.id);
            CHECK(s.min < s.max, s.id << " min < max");
            CHECK(s.def >= s.min && s.def <= s.max, s.id << " default in range");
            CHECK(s.skew > 0.f, s.id << " skew > 0");
            if (s.kind == Kind::Choice) CHECK(countChoices(s.choices) == static_cast<int>(s.max) + 1, s.id << " choice count matches range");
            for (const char* c = s.id; *c; ++c) CHECK((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_', s.id << " id characters");
        }
        INFO("parameters: " << kParamCount << " (" << kGlobalCount << " global + " << 3 * kBands << " per-band)");
        CHECK(kParamCount == kGlobalCount + 3 * kBands, "param count");
        CHECK(std::string(spec(kBandOn).id) == "b01_on" && std::string(spec(kBandTrim + 16).id) == "b17_trim", "band ids");
    }
    // 2. Defaults map to the engine defaults.
    {
        float v[kParamCount]; defaultValues(v); Controls c = Controls::fromValues(v);
        Controls d;
        CHECK(std::abs(c.fundamentalHz - d.fundamentalHz) < 1e-6 && c.tet == d.tet && c.harmDiv == d.harmDiv && std::abs(c.gravity - d.gravity) < 1e-6, "table defaults equal Controls defaults");
        CHECK(c.chaosMode == d.chaosMode && c.chaosType == d.chaosType && std::abs(c.level - d.level) < 1e-6 && c.drone && c.followMidi && !c.freeze, "toggle defaults");
        for (int i = 0; i < kBands; ++i) CHECK(c.bandOn[i] && c.bandLevel[i] == 1.0 && c.bandTrim[i] == 0.0, "band defaults");
        Instrument ins; ins.prepare(48000.0);
        CHECK(ins.engine().params().harmonicDivisor == 4 && ins.engine().params().tet == 12, "engine defaults after prepare");
    }
    // 3. Mapping, clamping, MIDI.
    {
        float v[kParamCount]; defaultValues(v);
        v[Coherence] = 2; v[ChaosMode] = 2; v[ChaosType] = 0; v[Gravity] = 5.0f; v[Octave] = -20.f; v[GroupLow] = std::nanf("");
        Controls c = Controls::fromValues(v);
        CHECK(c.gravity == 1.0, "values clamp to range"); CHECK(c.octave == -8, "int clamp"); CHECK(c.groupLow == 1.0, "NaN falls back to default");
        Instrument ins; ins.prepare(48000.0); ins.setControls(c);
        auto& p = ins.engine().params();
        CHECK(p.coherenceMult == 100, "choice x100"); CHECK(p.mode == cosmic::dsp::CarrierField::ChaosMode::Unstable, "mode map");
        CHECK(p.type == cosmic::dsp::CarrierField::ChaosType::Flicker, "type map");
        CHECK(std::abs(midiNoteToHz(69) - 440.0) < 1e-9 && std::abs(midiNoteToHz(57) - 220.0) < 1e-9, "mtof");
        v[Coherence] = 0; v[Octave] = 0; c = Controls::fromValues(v); ins.setControls(c);
        ins.noteOn(57); ins.setControls(c);
        CHECK(std::abs(ins.currentFundamental() - 220.0) < 1e-9, "MIDI note sets fundamental");
        v[FollowMidi] = 0; c = Controls::fromValues(v); ins.setControls(c);
        CHECK(std::abs(ins.currentFundamental() - 55.0) < 1e-9, "Follow MIDI off ignores the note");
        v[FollowMidi] = 1; c = Controls::fromValues(v); ins.clearNote(); ins.setControls(c);
        CHECK(std::abs(ins.currentFundamental() - 55.0) < 1e-9, "cleared note falls back to parameter");
    }
    // 4. Drone on/off and non-silence.
    {
        float v[kParamCount]; defaultValues(v);
        Instrument ins; ins.prepare(48000.0); ins.setControls(Controls::fromValues(v));
        std::vector<float> l(48000), r(48000);
        ins.render(l.data(), r.data(), 48000);
        double rms = 0; for (int i = 4800; i < 48000; ++i) rms += l[i] * l[i]; rms = std::sqrt(rms / 43200);
        INFO("default drone rms " << rms);
        CHECK(rms > 0.02, "drone must sound by default: " << rms);
        v[Drone] = 0; ins.setControls(Controls::fromValues(v)); ins.render(l.data(), r.data(), 48000);
        double tail = 0; for (int i = 24000; i < 48000; ++i) tail = std::max(tail, double(std::abs(l[i])));
        CHECK(tail < 1e-4, "drone off must be silent: " << tail);
        v[Drone] = 1; ins.setControls(Controls::fromValues(v)); ins.render(l.data(), r.data(), 48000);
        double back = 0; for (int i = 24000; i < 48000; ++i) back = std::max(back, double(std::abs(l[i])));
        CHECK(back > 0.05, "drone back on must sound: " << back);
    }
    // 5. Block-size independence (constant controls).
    {
        float v[kParamCount]; defaultValues(v); v[Gravity] = 0.8f; v[ChaosAmount] = 0.5f; v[FoldAmount] = 0.4f;
        std::vector<std::vector<float>> outs;
        for (int bs : { 1, 7, 64, 512, 1000 }) {
            Instrument ins; ins.prepare(48000.0); ins.setControls(Controls::fromValues(v));
            std::vector<float> o(24000), tmp(1000);
            for (int pos = 0; pos < 24000; pos += bs) { const int n = std::min(bs, 24000 - pos); ins.render(o.data() + pos, tmp.data(), n); }
            outs.push_back(o);
        }
        for (size_t k = 1; k < outs.size(); ++k) CHECK(outs[k] == outs[0], "output depends on block size (index " << k << ")");
    }
    // 6. Extremes and fuzz: min / max / random vectors, with random mid-run jumps. Never NaN, always bounded.
    {
        std::mt19937 rng(1234);
        auto randomVec = [&](float* v) {
            for (int i = 0; i < kParamCount; ++i) { const auto& s = spec(i); std::uniform_real_distribution<float> d(s.min, s.max); v[i] = d(rng); }
        };
        auto check = [&](Instrument& ins, int blocks, const char* tag, float* v, bool jump) {
            std::vector<float> l(480), r(480); double worst = 0; bool bad = false;
            for (int b = 0; b < blocks; ++b) {
                if (jump && b % 5 == 0) randomVec(v);
                ins.setControls(Controls::fromValues(v));
                if (b % 11 == 0) ins.noteOn(int(rng() % 128));
                ins.render(l.data(), r.data(), 480);
                for (float x : l) { if (!std::isfinite(x) || std::abs(x) > 1.0f) bad = true; worst = std::max(worst, double(std::abs(x))); }
            }
            CHECK(!bad, tag << ": non-finite or out-of-range output (peak " << worst << ")");
        };
        float v[kParamCount];
        for (int i = 0; i < kParamCount; ++i) v[i] = spec(i).min;
        { Instrument ins; ins.prepare(48000.0); check(ins, 100, "all minimum", v, false); }
        for (int i = 0; i < kParamCount; ++i) v[i] = spec(i).max;
        { Instrument ins; ins.prepare(48000.0); check(ins, 100, "all maximum", v, false); }
        for (int sr : { 22050, 44100, 48000, 96000, 192000 }) { defaultValues(v); Instrument ins; ins.prepare(sr); check(ins, 60, "sample rate", v, false); }
        for (int t = 0; t < 40; ++t) { randomVec(v); Instrument ins; ins.prepare(48000.0); check(ins, 60, "random vector", v, true); }
        INFO("fuzz: 40 random runs with parameter jumps every 50 ms, plus min/max/sample-rate sweeps: all finite and bounded");
    }
    std::cout << "Plugin core tests: " << (fails ? "FAIL" : "PASS") << " (" << fails << " failures)\n";
    return fails ? 1 : 0;
}
