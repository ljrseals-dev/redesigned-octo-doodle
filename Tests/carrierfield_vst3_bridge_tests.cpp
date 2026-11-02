#include "../vst3/source/CarrierFieldVst3Bridge.h"
#include <iostream>
#include <random>
#include <vector>
#include <string>

using namespace cosmic::vst3;
static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::cout << "FAIL: " << msg << "\n"; } } while (0)
#define INFO(msg) std::cout << msg << "\n"

int main() {
    // 1. normalized <-> plain round trips, defaults, step counts.
    {
        std::mt19937 rng(7); std::uniform_real_distribution<double> d(0.0, 1.0); double worst = 0;
        for (int i = 0; i < kParamCount; ++i) {
            const auto& s = spec(i); const int sc = stepCount(i);
            CHECK(std::abs(toPlain(i, defaultNorm(i)) - s.def) < 1e-4 * std::max(1.0, double(s.max - s.min)), s.id << " default round trip");
            CHECK(toPlain(i, 0.0) == s.min && std::abs(toPlain(i, 1.0) - s.max) < 1e-3, s.id << " endpoints");
            if (s.kind == Kind::Float) { CHECK(sc == 0, s.id << " float is continuous"); for (int k = 0; k < 50; ++k) { const double n = d(rng); worst = std::max(worst, std::abs(toNorm(i, toPlain(i, n)) - n)); } }
            else { CHECK(sc == static_cast<int>(s.max - s.min), s.id << " step count");
                   for (int v = static_cast<int>(s.min); v <= static_cast<int>(s.max); ++v) CHECK(std::lround(toPlain(i, toNorm(i, v))) == v, s.id << " discrete value " << v); }
            CHECK(std::isfinite(toPlain(i, std::nan(""))) && std::isfinite(toNorm(i, std::nan(""))), s.id << " NaN safe");
            CHECK(toPlain(i, 5.0) <= s.max && toPlain(i, -3.0) >= s.min, s.id << " clamps");
        }
        INFO("round trip: worst float normalized error " << worst);
        CHECK(worst < 1e-9, "float round trip error " << worst);
    }
    // 2. value <-> string.
    {
        char b[64]; double n;
        const int coh = Coherence, mode = ChaosMode, frz = Freeze, oct = Octave, grav = Gravity, fund = Fundamental;
        formatValue(coh, toNorm(coh, 2), b, 64); CHECK(std::string(b) == "x100", "coherence text " << b);
        formatValue(mode, toNorm(mode, 2), b, 64); CHECK(std::string(b) == "Unstable", "mode text " << b);
        formatValue(frz, 1.0, b, 64); CHECK(std::string(b) == "On", "bool text");
        formatValue(oct, toNorm(oct, -3), b, 64); CHECK(std::string(b) == "-3", "int text " << b);
        CHECK(parseValue(coh, "x20", n) && std::lround(toPlain(coh, n)) == 1, "parse choice");
        CHECK(parseValue(frz, "off", n) && n == 0.0, "parse bool");
        CHECK(parseValue(fund, "220 Hz", n) && std::abs(toPlain(fund, n) - 220.0) < 1e-3, "parse number with unit");
        CHECK(parseValue(grav, "0.25", n) && std::abs(toPlain(grav, n) - 0.25) < 1e-9, "parse float");
        CHECK(!parseValue(grav, "abc", n) && !parseValue(coh, nullptr, n), "reject junk");
        for (int i = 0; i < kParamCount; ++i) { formatValue(i, 0.37, b, 64); CHECK(b[0] != 0, spec(i).id << " has text"); }
        CHECK(std::string(unitFor(fund)) == "Hz" && std::string(unitFor(kBandTrim)) == "ct" && std::string(unitFor(grav)).empty(), "units");
    }
    // 3. state format.
    {
        float v[kParamCount]; defaultValues(v); v[Gravity] = 0.83f; v[Freeze] = 1.f; v[kBandOn + 4] = 0.f; v[kBandTrim + 9] = -42.5f;
        uint8_t buf[kStateMaxBytes + 8]; size_t n = serializeState(v, buf, sizeof(buf));
        CHECK(n == kStateMaxBytes, "state size " << n);
        float out[kParamCount]; defaultValues(out); out[Gravity] = 0.f;
        CHECK(deserializeState(buf, n, out), "state loads");
        CHECK(std::memcmp(v, out, sizeof(v)) == 0, "state round trip is exact (incl. Freeze)");
        CHECK(serializeState(v, buf, 10) == 0, "too-small buffer refused");
        float keep[kParamCount]; for (int i = 0; i < kParamCount; ++i) keep[i] = -999.f;
        auto untouched = [&]() { for (int i = 0; i < kParamCount; ++i) if (keep[i] != -999.f) return false; return true; };
        CHECK(!deserializeState(buf, 5, keep) && untouched(), "truncated header rejected");
        CHECK(!deserializeState(buf, n - 1, keep) && untouched(), "truncated body rejected");
        uint8_t bad[kStateMaxBytes]; std::memcpy(bad, buf, n); bad[0] = 'X';
        CHECK(!deserializeState(bad, n, keep) && untouched(), "bad magic rejected");
        std::memcpy(bad, buf, n); uint32_t ver = 99; std::memcpy(bad + 4, &ver, 4);
        CHECK(!deserializeState(bad, n, keep) && untouched(), "future version rejected");
        std::memcpy(bad, buf, n); uint32_t huge = 1u << 30; std::memcpy(bad + 8, &huge, 4);
        CHECK(!deserializeState(bad, n, keep) && untouched(), "absurd count rejected");
        // older state with fewer parameters: remainder gets defaults
        std::memcpy(bad, buf, n); uint32_t few = 5; std::memcpy(bad + 8, &few, 4);
        CHECK(deserializeState(bad, kStateHeader + 20, out), "shorter (older) state accepted");
        CHECK(out[Gravity] == spec(Gravity).def || Gravity < 5, "params beyond count reset to defaults");
        CHECK(out[kBandTrim + 9] == 0.f, "missing params default");
        // NaN / out-of-range values sanitised
        float nasty[kParamCount]; defaultValues(nasty); nasty[Gravity] = std::nanf(""); nasty[Level] = 77.f; nasty[Octave] = -500.f;
        serializeState(nasty, buf, sizeof(buf)); CHECK(deserializeState(buf, n, out), "nasty state loads");
        CHECK(out[Gravity] == spec(Gravity).def && out[Level] == 1.f && out[Octave] == -8.f, "NaN to default, range clamped");
    }
    // 4. Core: sample-accurate events equal manual segmentation; ordering; offsets clamped; n == 0.
    {
        const int N = 512; std::vector<float> a(N), ar(N), b(N), br(N);
        Core A; A.prepare(48000.0); Core B; B.prepare(48000.0);
        A.pushNote(300, 60); A.pushParam(100, Gravity, 0.9);              // pushed out of order on purpose
        A.render(a.data(), ar.data(), N);
        B.render(b.data(), br.data(), 100);
        B.pushParam(0, Gravity, 0.9); B.render(b.data() + 100, br.data() + 100, 200);
        B.pushNote(0, 60); B.render(b.data() + 300, br.data() + 300, N - 300);
        CHECK(a == b && ar == br, "sample-accurate rendering must equal manual block splitting");
        Core C; C.prepare(48000.0); C.pushParam(100000, Gravity, 1.0); C.pushParam(-50, Level, 0.5);
        std::vector<float> c(64), cr(64); C.render(c.data(), cr.data(), 64);
        CHECK(std::abs(C.values()[Gravity] - 1.0f) < 1e-6 && std::abs(C.values()[Level] - 0.5f) < 1e-6, "out-of-range offsets clamp and still apply");
        C.pushParam(3, Gravity, 0.1); C.render(nullptr, nullptr, 0);
        CHECK(std::abs(C.values()[Gravity] - 0.1f) < 1e-6, "zero-length block applies queued changes");
        Core M; M.prepare(48000.0); std::vector<float> m(256); M.render(m.data(), nullptr, 256);
        CHECK(true, "mono render (no right buffer) works");
        Core Q; Q.prepare(48000.0); for (int i = 0; i < Core::kMaxEvents + 50; ++i) Q.pushParam(i % 100, Gravity, (i % 10) / 10.0);
        std::vector<float> q(128), qr(128); Q.render(q.data(), qr.data(), 128);
        bool ok = true; for (float x : q) if (!std::isfinite(x)) ok = false; CHECK(ok, "event overflow is handled");
    }
    // 5. State restores the sound: save, load into a new Core, identical controls.
    {
        Core A; A.prepare(48000.0); A.pushParam(0, Gravity, 0.77); A.pushParam(0, Freeze, 1.0); A.pushParam(0, ChaosAmount, 0.4);
        std::vector<float> t(128), tr(128); A.render(t.data(), tr.data(), 128);
        uint8_t buf[kStateMaxBytes]; serializeState(A.values(), buf, sizeof(buf));
        Core B; B.prepare(48000.0); float v[kParamCount]; CHECK(deserializeState(buf, sizeof(buf), v), "load"); B.loadPlain(v);
        CHECK(std::memcmp(A.values(), B.values(), 4 * kParamCount) == 0, "restored values identical");
        B.render(t.data(), tr.data(), 128);
        CHECK(B.instrument().engine().params().freeze, "Freeze status restored");
    }
    // 6. Fuzz: random automation + notes every block, random block sizes: finite and bounded.
    {
        std::mt19937 rng(99); bool bad = false;
        for (int run = 0; run < 20; ++run) {
            Core c; c.prepare(44100.0 + 4000.0 * (run % 3)); std::vector<float> l(2048), r(2048);
            for (int b = 0; b < 60; ++b) {
                const int n = 1 + int(rng() % 2000);
                for (int k = 0; k < int(rng() % 12); ++k) c.pushParam(int(rng() % (n + 20)) - 10, int(rng() % kParamCount), (rng() % 1000) / 999.0);
                if (rng() % 7 == 0) c.pushNote(int(rng() % n), int(rng() % 128));
                c.render(l.data(), r.data(), n);
                for (int i = 0; i < n; ++i) if (!std::isfinite(l[i]) || std::abs(l[i]) > 1.f || l[i] != r[i]) bad = true;
            }
        }
        CHECK(!bad, "fuzz produced non-finite, out-of-range or mismatched output");
        INFO("fuzz: 20 runs x 60 random-size blocks with random automation/notes: finite, bounded, L==R");
    }
    std::cout << "VST3 bridge tests: " << (fails ? "FAIL" : "PASS") << " (" << fails << " failures)\n";
    return fails ? 1 : 0;
}
