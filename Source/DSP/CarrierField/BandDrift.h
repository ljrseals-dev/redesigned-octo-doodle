#pragma once
/**
 * BandDrift - C++ port of the analog-drift model (infamous-casket / analog-drift core.js).
 * Output is pitch deviation in cents, built from frequency-noise sources:
 *   slow    1/f ("flicker") FM from octave-spaced one-pole filters, 0.02..3 Hz
 *   wander  leaky random-walk FM (thermal wander), bounded by wanderTime
 *   jitter  white FM low-passed at 200 Hz
 *   warmup  exponential settling after power-on
 *   spread  fixed per-instance offset drawn once from the seed
 * Runs at a fixed 1.5 kHz model rate; advance() is linearly interpolated and
 * takes a speed multiplier. Allocation-free, deterministic for a given seed.
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cosmic::dsp {

class DriftRng {
public:
    explicit DriftRng(uint32_t seed = 1) noexcept { reseed(seed); }
    void reseed(uint32_t seed) noexcept { s_ = seed ? seed : 1u; hasSpare_ = false; }

    double uniform() noexcept { // mulberry32
        s_ += 0x6D2B79F5u;
        uint32_t t = s_;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
    }
    double gauss() noexcept { // polar Box-Muller with spare
        if (hasSpare_) { hasSpare_ = false; return spare_; }
        double u, v, q;
        do { u = uniform() * 2.0 - 1.0; v = uniform() * 2.0 - 1.0; q = u * u + v * v; }
        while (q >= 1.0 || q == 0.0);
        const double m = std::sqrt(-2.0 * std::log(q) / q);
        spare_ = v * m; hasSpare_ = true;
        return u * m;
    }
private:
    uint32_t s_ = 1;
    double spare_ = 0.0;
    bool hasSpare_ = false;
};

struct BandDriftParams {
    double slow = 2.5;       // rms cents, 1/f component
    double wander = 1.5;     // rms cents, random walk
    double wanderTime = 20.0;// seconds
    double jitter = 0.25;    // rms cents, white FM (200 Hz LP)
    double warmup = 0.0;     // cents at t=0, decays
    double warmupTime = 25.0;
    double spread = 0.0;     // rms cents of fixed offset
    double depth = 1.0;      // global scale
};

class BandDrift {
public:
    static constexpr double kControlRate = 1500.0;

    BandDrift() { buildCoefs(); }

    void setParams(const BandDriftParams& p) noexcept { p_ = p; }
    void setDepth(double d) noexcept { p_.depth = d; }
    const BandDriftParams& params() const noexcept { return p_; }

    void reset(uint32_t seed) noexcept {
        rng_.reseed(seed);
        pink_.fill(0.0); rw_ = 0.0; jl_ = 0.0; t_ = 0.0;
        spreadCents_ = rng_.gauss() * p_.spread;
        frac_ = 0.0;
        cur_ = tick(); prev_ = cur_;
    }

    /** Advance the model by `ticks` control steps (1.0 = one 1.5 kHz step). Returns interpolated cents. */
    double advance(double ticks) noexcept {
        frac_ += ticks;
        while (frac_ >= 1.0) { prev_ = cur_; cur_ = tick(); frac_ -= 1.0; }
        return prev_ + (cur_ - prev_) * frac_;
    }
    double value() const noexcept { return prev_ + (cur_ - prev_) * frac_; }

    /** One raw model step (exposed for spectral tests). */
    double tick() noexcept {
        const double dt = 1.0 / kControlRate;
        double pk = 0.0;
        for (size_t i = 0; i < kPink; ++i) {
            pink_[i] += pa_[i] * (rng_.gauss() - pink_[i]);
            pk += pink_[i] * pnorm_[i];
        }
        pk = (pk / std::sqrt(static_cast<double>(kPink))) * p_.slow;
        const double tau = std::max(0.5, p_.wanderTime);
        const double sigma = p_.wander * std::sqrt(2.0 / tau);
        rw_ += -rw_ * dt / tau + sigma * std::sqrt(dt) * rng_.gauss();
        const double aj = std::min(0.5, (2.0 * 3.14159265358979323846 * 200.0) / kControlRate);
        jl_ += aj * (rng_.gauss() - jl_);
        const double jt = (jl_ / std::sqrt(aj / (2.0 - aj))) * p_.jitter;
        const double warm = p_.warmup * std::exp(-t_ / std::max(0.1, p_.warmupTime));
        t_ += dt;
        return (spreadCents_ + warm + pk + rw_ + jt) * p_.depth;
    }

private:
    static constexpr size_t kPink = 8; // 0.02,0.04,...,2.56 Hz (same octave set as core.js)

    void buildCoefs() noexcept {
        double f = 0.02;
        for (size_t i = 0; i < kPink; ++i, f *= 2.0) {
            const double a = std::min(0.5, (2.0 * 3.14159265358979323846 * f) / kControlRate);
            pa_[i] = a;
            pnorm_[i] = 1.0 / std::sqrt(a / (2.0 - a));
        }
    }

    BandDriftParams p_;
    DriftRng rng_;
    std::array<double, kPink> pink_{}, pa_{}, pnorm_{};
    double rw_ = 0.0, jl_ = 0.0, t_ = 0.0, spreadCents_ = 0.0;
    double frac_ = 0.0, prev_ = 0.0, cur_ = 0.0;
};

} // namespace cosmic::dsp
