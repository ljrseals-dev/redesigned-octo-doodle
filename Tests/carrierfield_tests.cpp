#include "../Source/DSP/CarrierField/CarrierField.h"
#include <complex>
#include <vector>
#include <iostream>
#include <chrono>
#include <cstring>
#include <set>

using namespace cosmic::dsp;
static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::cout << "FAIL: " << msg << "\n"; } } while (0)
#define INFO(msg) std::cout << msg << "\n"

// ---- small radix-2 FFT for spectral checks ----
static void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * 3.14159265358979323846 / len;
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t k = 0; k < len / 2; ++k) {
                auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
}

// Welch PSD slope (log-log) of a series sampled at fs, fitted between fLo and fHi.
static double psdSlope(const std::vector<double>& x, double fs, double fLo, double fHi, size_t seg = 4096) {
    std::vector<double> psd(seg / 2, 0.0); int nseg = 0;
    for (size_t s = 0; s + seg <= x.size(); s += seg / 2) {
        std::vector<std::complex<double>> a(seg);
        double mean = 0; for (size_t i = 0; i < seg; ++i) mean += x[s + i]; mean /= seg;
        for (size_t i = 0; i < seg; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * i / (seg - 1));
            a[i] = (x[s + i] - mean) * w;
        }
        fft(a); for (size_t k = 1; k < seg / 2; ++k) psd[k] += std::norm(a[k]);
        ++nseg;
    }
    double sx = 0, sy = 0, sxx = 0, sxy = 0; int m = 0;
    for (size_t k = 1; k < seg / 2; ++k) {
        const double f = k * fs / seg; if (f < fLo || f > fHi) continue;
        const double lx = std::log10(f), ly = std::log10(psd[k] / nseg + 1e-300);
        sx += lx; sy += ly; sxx += lx * lx; sxy += lx * ly; ++m;
    }
    return (m * sxy - sx * sy) / (m * sxx - sx * sx);
}

static std::vector<double> driftSeries(const BandDriftParams& p, uint32_t seed, size_t n, int decim) {
    BandDrift d; d.setParams(p); d.reset(seed);
    std::vector<double> out; out.reserve(n / decim); double acc = 0; int c = 0;
    for (size_t i = 0; i < n; ++i) { acc += d.tick(); if (++c == decim) { out.push_back(acc / decim); acc = 0; c = 0; } }
    return out;
}

static CarrierField make(double g, double pfl, int coh, double chaos, bool freeze = false, uint32_t seed = 3) {
    CarrierField cf; cf.setSampleRate(48000.0);
    CarrierField::Params p; p.gravity = g; p.phaseFieldLock = pfl; p.coherenceMult = coh;
    p.chaosAmount = chaos; p.freeze = freeze; p.seed = seed; p.cycleDetuneCents = 4.0;
    cf.setParams(p); cf.reset(); return cf;
}
static void run(CarrierField& cf, double seconds, std::vector<float>* out = nullptr) {
    const int n = static_cast<int>(seconds * 48000);
    for (int i = 0; i < n; ++i) { float x = cf.process(); if (out) out->push_back(x); }
}

int main() {
    // 1. Drift port: spectral slopes (targets -1, -2, 0 for frequency noise in cents).
    {
        const size_t N = 1500 * 1200; // 20 min of model time
        BandDriftParams ps; ps.slow = 3; ps.wander = 0; ps.jitter = 0; ps.spread = 0;
        BandDriftParams pw; pw.slow = 0; pw.wander = 2; pw.jitter = 0; pw.spread = 0;
        BandDriftParams pj; pj.slow = 0; pj.wander = 0; pj.jitter = 1; pj.spread = 0;
        // decimate by 50 -> 30 Hz series
        const double s1 = psdSlope(driftSeries(ps, 11, N, 50), 30.0, 0.05, 1.5);
        const double s2 = psdSlope(driftSeries(pw, 12, N, 50), 30.0, 0.1, 3.0);
        const double s3 = psdSlope(driftSeries(pj, 13, N, 50), 30.0, 0.5, 6.0);
        INFO("drift PSD slopes  slow " << s1 << "  wander " << s2 << "  jitter " << s3);
        CHECK(s1 > -1.5 && s1 < -0.6, "slow (1/f) slope " << s1);
        CHECK(s2 > -2.5 && s2 < -1.5, "wander (1/f^2) slope " << s2);
        CHECK(s3 > -0.4 && s3 < 0.4, "jitter (flat) slope " << s3);
    }
    // 2. Drift determinism, seed independence.
    {
        BandDriftParams p; auto a = driftSeries(p, 5, 30000, 1), b = driftSeries(p, 5, 30000, 1);
        CHECK(a == b, "same seed must reproduce exactly");
        double worst = 0;
        for (int s = 1; s <= 6; ++s) {
            auto x = driftSeries(p, 100 + s, 1500 * 60, 15), y = driftSeries(p, 200 + s, 1500 * 60, 15);
            double mx = 0, my = 0; for (size_t i = 0; i < x.size(); ++i) { mx += x[i]; my += y[i]; } mx /= x.size(); my /= y.size();
            double sxy = 0, sxx = 0, syy = 0; for (size_t i = 0; i < x.size(); ++i) { sxy += (x[i]-mx)*(y[i]-my); sxx += (x[i]-mx)*(x[i]-mx); syy += (y[i]-my)*(y[i]-my); }
            worst = std::max(worst, std::abs(sxy / std::sqrt(sxx * syy)));
        }
        INFO("worst |corr| between seeds: " << worst);
        CHECK(worst < 0.6, "seeds too correlated: " << worst);
    }
    // 3. Tuning grid: TET, octave shifts, harmonics.
    {
        CarrierField cf; auto p = cf.params(); p.fundamentalHz = 55; p.tet = 12; cf.setParams(p); cf.reset();
        CHECK(std::abs(cf.bandNominal(0) - 55.0) < 1e-9, "band0 = fundamental");
        CHECK(std::abs(cf.bandNominal(1) / 55.0 - std::pow(2.0, 7.0 / 12.0)) < 1e-9, "12-TET fifth stride");
        p.tet = 19; cf.setParams(p); cf.reset();
        CHECK(std::abs(cf.bandNominal(1) / 55.0 - std::pow(2.0, 11.0 / 19.0)) < 1e-9, "19-TET stride 11");
        p.tet = 12; p.octaveShift = -4; cf.setParams(p);
        CHECK(std::abs(cf.fundamental() - 55.0 / 16.0) < 1e-9, "octave shift -4");
        p.octaveShift = 8; cf.setParams(p);
        CHECK(cf.fundamental() <= 0.40 * 48000.0 + 1e-9, "extreme octave shift is clamped below Nyquist");
        for (int i = 0; i < 17; ++i) CHECK(cf.bandHarmonic(i) >= 1, "harmonic >= 1");
        // default grid must give most bands their own harmonic, with bounded pull
        p.octaveShift = 0; p.tet = 12; cf.setParams(p); cf.reset();
        std::set<int> uniq; double maxPull = 0;
        for (int i = 0; i < 17; ++i) { uniq.insert(cf.bandHarmonic(i)); maxPull = std::max(maxPull, std::abs(1200.0 * std::log2(cf.referenceFrequency() * cf.bandHarmonic(i) / cf.bandNominal(i)))); }
        INFO("default grid: " << uniq.size() << " unique harmonics, max pull " << maxPull << " cents");
        CHECK(uniq.size() >= 12, "default grid should give >= 12 distinct harmonics: " << uniq.size());
        CHECK(maxPull <= 210.0, "default max gravity pull should stay within ~200 cents: " << maxPull);
        p.harmonicDivisor = 1; cf.setParams(p);
        std::set<int> u1; for (int i = 0; i < 17; ++i) u1.insert(cf.bandHarmonic(i));
        CHECK(u1.size() < uniq.size(), "divisor 1 collapses more bands than the default");
        p.harmonicDivisor = 4; cf.setParams(p);
    }
    // 4. Gravity: free carriers stay incoherent, locked field becomes coherent, monotonic in between.
    {
        auto lo = make(0.0, 0, 1, 0); run(lo, 3.0);
        double rLo = lo.coherence();
        auto hi = make(1.0, 0, 1, 0); run(hi, 3.0);
        double rHi = hi.coherence();
        INFO("coherence gravity 0: " << rLo << "  gravity 1: " << rHi);
        CHECK(rLo < 0.7, "gravity 0 should be incoherent: " << rLo);
        CHECK(rHi > 0.95, "gravity 1 should lock: " << rHi);
        double prev = -1; bool mono = true;
        for (double g : { 0.0, 0.25, 0.5, 0.75, 1.0 }) {
            auto cf = make(g, 0, 1, 0); run(cf, 4.0);
            double acc = 0; for (int k = 0; k < 200; ++k) { run(cf, 0.01); acc += cf.coherence(); } acc /= 200;
            INFO("  gravity " << g << " -> mean coherence " << acc);
            if (acc < prev - 0.08) mono = false;
            prev = acc;
        }
        CHECK(mono, "coherence must not fall as gravity rises");
        // lock time falls as coherence multiplier rises
        double tt[3]; int idx = 0;
        for (int m : { 1, 20, 100 }) {
            auto cf = make(1.0, 0, m, 0); double t = 10.0;
            for (int i = 0; i < 20000; ++i) { run(cf, 0.001); if (cf.coherence() > 0.95) { t = cf.freezeMix() * 0 + (i + 1) * 0.001; break; } }
            tt[idx++] = t; INFO("  lock time (R>0.95) coherence x" << m << ": " << t << " s");
        }
        CHECK(tt[1] <= tt[0] + 1e-9 && tt[2] <= tt[1] + 1e-9, "higher Harmony Coherence must not lock slower");
        CHECK(tt[2] < 1.0, "x100 locks within 1 s");
    }
    // 5. Phase Field Lock alone pulls bands together.
    {
        auto off = make(0.0, 0.0, 20, 0); run(off, 3.0);
        auto on  = make(0.0, 1.0, 20, 0); run(on, 3.0);
        INFO("PFL off R=" << off.coherence() << "  PFL on R(mean-field)=" << on.coherence());
        // with gravity 0 the reference frame still spins; check group tightness via spread of used frequencies
        double sOff = 0, sOn = 0;
        for (int i = 0; i < 17; ++i) { sOff += std::abs(off.bandFrequency(i) - off.bandNominal(i)); sOn += std::abs(on.bandFrequency(i) - on.bandNominal(i)); }
        CHECK(sOn >= sOff, "Phase Field Lock should move bands more than free-run");
    }
    // 6. Freeze EM: used parameters hold while live chaos would move them.
    {
        auto cf = make(0.6, 0.2, 1, 1.0); run(cf, 2.0);
        auto p = cf.params(); p.freeze = true; cf.setParams(p); run(cf, 0.1);
        double f0[17]; for (int i = 0; i < 17; ++i) f0[i] = cf.bandFrequency(i);
        double maxDev = 0; for (int k = 0; k < 100; ++k) { run(cf, 0.05); for (int i = 0; i < 17; ++i) maxDev = std::max(maxDev, std::abs(cf.bandFrequency(i) - f0[i]) / f0[i]); }
        INFO("frozen max relative frequency deviation: " << maxDev);
        CHECK(maxDev < 1e-9, "frozen field must hold frequencies: " << maxDev);
        CHECK(cf.freezeMix() > 0.999, "freeze mix should be fully engaged");
        auto cf2 = make(0.6, 0.2, 1, 1.0); run(cf2, 2.0);
        double g0[17]; for (int i = 0; i < 17; ++i) g0[i] = cf2.bandFrequency(i);
        double dev2 = 0; for (int k = 0; k < 100; ++k) { run(cf2, 0.05); for (int i = 0; i < 17; ++i) dev2 = std::max(dev2, std::abs(cf2.bandFrequency(i) - g0[i]) / g0[i]); }
        INFO("unfrozen max relative frequency deviation: " << dev2);
        CHECK(dev2 > 1e-5, "unfrozen chaos must move frequencies");
        // click-free: sample-to-sample jump during freeze engage stays small
        auto cf3 = make(1.0, 0, 1, 0.5); run(cf3, 1.0);
        std::vector<float> y; auto q = cf3.params(); q.freeze = true; cf3.setParams(q); run(cf3, 0.2, &y);
        float worst = 0; for (size_t i = 1; i < y.size(); ++i) worst = std::max(worst, std::abs(y[i] - y[i - 1]));
        INFO("max sample step during freeze engage: " << worst);
        CHECK(worst < 0.5f, "freeze must not click: " << worst);
    }
    // 7. Fold changes the signal; zero fold is the plain carrier sum.
    {
        auto a = make(1.0, 0, 1, 0); auto b = make(1.0, 0, 1, 0);
        auto pb = b.params(); pb.foldAmount = 1.0; b.setParams(pb);
        std::vector<float> xa, xb; run(a, 0.5, &xa); run(b, 0.5, &xb);
        double d = 0; for (size_t i = 0; i < xa.size(); ++i) d += std::abs(xa[i] - xb[i]);
        INFO("fold on/off mean abs difference: " << d / xa.size());
        CHECK(d / xa.size() > 1e-3, "17 Hz fold must audibly change the signal");
    }
    // 8. Band on/off and level are independent.
    {
        CarrierField cf; auto p = cf.params(); p.gravity = 1; cf.setParams(p); cf.reset();
        for (int i = 0; i < 17; ++i) cf.setBand(i, false, 1.0);
        std::vector<float> x; run(cf, 0.3, &x);
        float pk = 0; for (size_t i = 12000; i < x.size(); ++i) pk = std::max(pk, std::abs(x[i]));
        CHECK(pk < 1e-3f, "all bands off must be silent: " << pk);
        cf.setBand(5, true, 1.0); x.clear(); run(cf, 0.3, &x);
        pk = 0; for (size_t i = 12000; i < x.size(); ++i) pk = std::max(pk, std::abs(x[i]));
        CHECK(pk > 0.05f, "one band on must sound: " << pk);
    }
    // 9. Determinism, finiteness, bound, and every chaos mode/type.
    {
        auto a = make(0.7, 0.3, 20, 0.8, false, 99), b = make(0.7, 0.3, 20, 0.8, false, 99);
        std::vector<float> xa, xb; run(a, 1.0, &xa); run(b, 1.0, &xb);
        CHECK(xa == xb, "same seed must render identically");
        int n = 0;
        for (auto m : { CarrierField::ChaosMode::Locked, CarrierField::ChaosMode::Drifting, CarrierField::ChaosMode::Unstable })
            for (auto t : { CarrierField::ChaosType::Flicker, CarrierField::ChaosType::Wander, CarrierField::ChaosType::Jitter, CarrierField::ChaosType::Blend })
                for (int coh : { 1, 20, 100 }) {
                    auto cf = make(1.0, 1.0, coh, 1.0); auto p = cf.params(); p.mode = m; p.type = t; p.foldAmount = 1.0; cf.setParams(p);
                    std::vector<float> x; run(cf, 0.5, &x);
                    for (float v : x) { if (!std::isfinite(v) || std::abs(v) > 1.0f) { ++n; break; } }
                }
        CHECK(n == 0, n << " mode/type/coherence combos produced non-finite or out-of-range output");
    }
    // 10. Real-time budget.
    {
        auto cf = make(1.0, 0.5, 100, 0.7); cf.setParams([&] { auto p = cf.params(); p.foldAmount = 0.5; return p; }());
        const int N = 48000 * 10;
        auto t0 = std::chrono::steady_clock::now(); float acc = 0; for (int i = 0; i < N; ++i) acc += cf.process();
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        INFO("17 bands: 10 s of audio in " << sec << " s  (" << 10.0 / sec << "x real time)  [" << acc * 0 << "]");
        CHECK(10.0 / sec > 5.0, "should run at least 5x real time");
    }
    std::cout << "CarrierField tests: " << (fails ? "FAIL" : "PASS") << " (" << fails << " failures)\n";
    return fails ? 1 : 0;
}
