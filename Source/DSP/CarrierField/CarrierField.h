#pragma once
/**
 * CarrierField - 17-band carrier-field engine (drone / texture, no note logic).
 *
 * Signal path:
 *   17 band generators (64-bit PhaseAccumulator each)
 *     -> per-band 17 Hz phase fold
 *     -> band mixer (level / on / coherence-aware normalisation)
 *     -> Master Gravity + Phase Field Lock + Harmony Coherence (control-rate loop)
 *     -> Cycle Detune + Chaos (BandDrift, multi-rate) -> Freeze EM crossfade -> soft limit
 *
 * Interpretations (see Docs/CARRIER_FIELD.md):
 *   Master Gravity     pulls each band toward the nearest harmonic of the shared
 *                      reference f0/harmonicDivisor (frequency pull) and relaxes its phase toward the
 *                      harmonic phase h*Phi0 (phase pull). 0 = free, 1 = fully locked.
 *   Phase Field Lock   Kuramoto-style mean-field pull of each band toward its group's
 *                      circular-mean phase (low / mid / high groups).
 *   Harmony Coherence  loop-gain multiplier (x1 / x20 / x100). Pulls are exact
 *                      exponential relaxations per control tick, so large gains stay stable.
 *   Freeze EM          latches each band's frequency, fold depth, phase offset and level and
 *                      crossfades to them over ~20 ms; drift keeps running underneath so
 *                      unfreezing is click-free.
 *   17 Hz fold         per-band triangle phase-fold modulator at foldHz (default 17 Hz).
 * Real-time safe: no allocation, locks or I/O after setSampleRate().
 */

#include "BandDrift.h"
#include "../Oscillators/PhaseAccumulator.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace cosmic::dsp {

class CarrierField {
public:
    static constexpr int kBands = 17;
    static constexpr int kGroups = 3; // low 0-5, mid 6-11, high 12-16

    enum class ChaosMode { Locked, Drifting, Unstable };
    enum class ChaosType { Flicker, Wander, Jitter, Blend };

    struct Params {
        // Tuning
        double fundamentalHz = 55.0;
        int    octaveShift   = 0;      // extreme shifts allowed, clamped by frequency limits
        int    tet           = 12;     // divisions per octave (1..96)
        int    stepStride    = 0;      // TET steps between consecutive bands; 0 = nearest to a fifth
        int    spanOctaves   = 4;      // band ratios fold into [1, 2^span)
        int    harmonicDivisor = 4;    // gravity grid = partials of f0/D (1..16); 1 collapses many bands onto few harmonics
        // Field
        double gravity       = 0.5;    // 0..1
        double phaseFieldLock= 0.0;    // 0..1
        int    coherenceMult = 1;      // 1, 20 or 100
        std::array<double, kGroups> groupCoherence { 1.0, 1.0, 1.0 };
        // Cycle detune
        double cycleDetuneCents = 6.0; // overall depth
        double cycleSpeed       = 0.3; // 0..1 -> 0.05x .. 4x model speed
        // Chaos
        ChaosMode mode   = ChaosMode::Drifting;
        ChaosType type   = ChaosType::Blend;
        double chaosAmount = 0.0;      // 0..1
        // Fold
        double foldAmount = 0.0;       // 0..1
        double foldHz     = 17.0;
        // Output
        double masterLevel = 0.6;
        bool   freeze = false;
        uint32_t seed = 1;
    };

    CarrierField() { setSampleRate(48000.0); applyDriftParams(); reset(); }

    void setSampleRate(double sr) noexcept {
        sampleRate_ = std::max(8000.0, sr);
        stride_ = std::max(1, static_cast<int>(std::lround(sampleRate_ / BandDrift::kControlRate)));
        dt_ = stride_ / sampleRate_;
        master_.setSampleRate(sampleRate_);
        for (auto& b : bands_) b.acc.setSampleRate(sampleRate_);
        fzStep_ = 1.0 / std::max(1.0, 0.020 * sampleRate_);
        ampCoef_ = 1.0 - std::exp(-1.0 / (0.010 * sampleRate_));
        updateGrid();
    }

    void setParams(const Params& p) noexcept {
        const bool freezeEdge = p.freeze && !params_.freeze;
        const bool gridChanged = p.fundamentalHz != params_.fundamentalHz || p.octaveShift != params_.octaveShift
            || p.tet != params_.tet || p.stepStride != params_.stepStride || p.spanOctaves != params_.spanOctaves
            || p.harmonicDivisor != params_.harmonicDivisor;
        params_ = p;
        params_.tet = std::clamp(params_.tet, 1, 96);
        params_.spanOctaves = std::clamp(params_.spanOctaves, 1, 10);
        params_.harmonicDivisor = std::clamp(params_.harmonicDivisor, 1, 16);
        params_.coherenceMult = (params_.coherenceMult >= 100) ? 100 : (params_.coherenceMult >= 20 ? 20 : 1);
        if (freezeEdge) latch();
        if (gridChanged) updateGrid();
        applyDriftParams();
    }
    const Params& params() const noexcept { return params_; }

    // Per-band controls (independent on/off, level, carrier trim in cents)
    void setBand(int i, bool on, double level, double trimCents = 0.0) noexcept {
        if (i < 0 || i >= kBands) return;
        bands_[i].on = on; bands_[i].level = std::clamp(level, 0.0, 1.0);
        if (bands_[i].trimCents != trimCents) { bands_[i].trimCents = trimCents; updateGrid(); }
    }

    /** Deterministic restart: phases, drift and loop state derive from params.seed. */
    void reset() noexcept {
        uint64_t s = 0x9E3779B97F4A7C15ull ^ (static_cast<uint64_t>(params_.seed) * 0xBF58476D1CE4E5B9ull);
        master_.reset(0.0);
        for (int i = 0; i < kBands; ++i) {
            auto& b = bands_[i];
            b.acc.setRawPhase(splitmix(s));
            b.foldPhase = (i + 0.5) / kBands;
            b.cyc.reset(hash32(params_.seed, i, 1));
            b.chaos.reset(hash32(params_.seed, i, 2));
            b.amp = 0.0;
            b.live = { b.nominal, 0.0, 0.0, b.on ? b.level : 0.0 };
            b.phaseS = 0.0;
            b.latched = b.live;
            b.df = 0.0; b.cycCents = 0.0; b.chaosCents = 0.0; b.theta = 0.0;
        }
        fz_ = params_.freeze ? 1.0 : 0.0;
        cycDepthS_ = std::max(0.0, params_.cycleDetuneCents) / 4.0;
        chaosAmtS_ = std::clamp(params_.chaosAmount, 0.0, 1.0);
        driftScaleS_ = driftScale(); loopScaleS_ = loopScale();
        foldS_ = std::clamp(params_.foldAmount, 0.0, 1.0); levelS_ = params_.masterLevel;
        for (auto& b : bands_) { b.cyc.setDepth(cycDepthS_); b.chaos.setDepth(chaosAmtS_ * driftScaleS_); b.phaseS = 0.0; }
        ctrlCounter_ = 0; coherence_ = 0.0; coherenceSmooth_ = 0.0; latchedCoh_ = 0.0;
        controlTick();
        for (auto& b : bands_) { b.acc.setFrequency(b.usedFreq); b.amp = 0.0; } // 10 ms fade-in: no start click
    }

    float process() noexcept {
        if (ctrlCounter_ == 0) controlTick();
        if (++ctrlCounter_ >= stride_) ctrlCounter_ = 0;

        levelS_ += ampCoef_ * (params_.masterLevel - levelS_);
        fz_ += (params_.freeze ? fzStep_ : -fzStep_);
        fz_ = std::clamp(fz_, 0.0, 1.0);

        master_.tick();
        const double foldInc = params_.foldHz / sampleRate_;
        double sum = 0.0;
        for (auto& b : bands_) {
            const double ph = b.acc.tick();
            b.foldPhase += foldInc; b.foldPhase -= std::floor(b.foldPhase);
            const double tri = 4.0 * std::abs(b.foldPhase - 0.5) - 1.0; // [-1,1]
            b.amp += ampCoef_ * (b.usedLevel - b.amp);
            const double arg = ph + b.usedPhaseOff + b.usedFold * tri;
            sum += b.amp * std::sin(6.283185307179586 * arg);
        }
        const double n = std::max(1.0, static_cast<double>(onCount_));
        const double rn = std::sqrt(n);
        const double usedCoh = coherenceSmooth_ + (latchedCoh_ - coherenceSmooth_) * fz_; // frozen field keeps its level
        const double norm = 1.0 / (rn * (1.0 + (rn - 1.0) * usedCoh));
        return static_cast<float>(softLimit(sum * norm * levelS_));
    }

    // ---- introspection (tests / UI) ----
    double coherence() const noexcept { return coherence_; }          // |mean exp(i 2pi theta)| over active bands
    double bandFrequency(int i) const noexcept { return bands_[i].usedFreq; }
    double bandNominal(int i) const noexcept { return bands_[i].nominal; }
    int    bandHarmonic(int i) const noexcept { return bands_[i].harmonic; }
    double bandTheta(int i) const noexcept { return bands_[i].theta; } // cycles, vs harmonic reference
    double fundamental() const noexcept { return f0_; }
    double referenceFrequency() const noexcept { return fRef_; } // f0 / harmonicDivisor
    double freezeMix() const noexcept { return fz_; }
    // Everything the output depends on besides time: constant while frozen => output is a fixed sum of sinusoids.
    double bandAmplitude(int i) const noexcept { return bands_[i].amp; }
    double bandPhaseOffset(int i) const noexcept { return bands_[i].usedPhaseOff; }  // cycles
    double bandFoldDepth(int i) const noexcept { return bands_[i].usedFold; }        // cycles
    double outputNorm() const noexcept {
        const double rn = std::sqrt(std::max(1.0, static_cast<double>(onCount_)));
        const double c = coherenceSmooth_ + (latchedCoh_ - coherenceSmooth_) * fz_;
        return levelS_ / (rn * (1.0 + (rn - 1.0) * c));
    }

private:
    struct Live { double freq, fold, phaseOff, level; };
    struct Band {
        PhaseAccumulator acc;
        BandDrift cyc, chaos;
        bool on = true; double level = 1.0, trimCents = 0.0;
        double nominal = 55.0, target = 55.0; int harmonic = 1;
        double foldPhase = 0.0, amp = 0.0;
        double cycCents = 0.0, chaosCents = 0.0, df = 0.0, theta = 0.0, phaseS = 0.0;
        Live live{}, latched{};
        double usedFreq = 55.0, usedFold = 0.0, usedPhaseOff = 0.0, usedLevel = 0.0;
    };

    static uint64_t splitmix(uint64_t& s) noexcept {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    static uint32_t hash32(uint32_t seed, int band, int kind) noexcept {
        uint64_t s = (static_cast<uint64_t>(seed) << 32) ^ (static_cast<uint64_t>(band) * 2654435761ull) ^ (static_cast<uint64_t>(kind) << 8);
        return static_cast<uint32_t>(splitmix(s) >> 32) | 1u;
    }
    // Transparent below 0.9; smooth knee above, asymptote 1.0 (no distortion in normal use).
    static double softLimit(double x) noexcept {
        const double a = std::abs(x), k = 0.9;
        if (a <= k) return x;
        const double y = k + (1.0 - k) * std::tanh((a - k) / (1.0 - k));
        return x < 0 ? -y : y;
    }
    static int groupOf(int i) noexcept { return i < 6 ? 0 : (i < 12 ? 1 : 2); }
    static double wrap01(double x) noexcept { return x - std::floor(x + 0.5); } // -> [-0.5,0.5)

    void updateGrid() noexcept {
        f0_ = std::clamp(params_.fundamentalHz * std::pow(2.0, params_.octaveShift), 0.05, 0.40 * sampleRate_);
        fRef_ = f0_ / params_.harmonicDivisor;
        master_.setFrequency(fRef_);
        const int tet = params_.tet;
        int stride = params_.stepStride;
        if (stride <= 0) stride = std::max(1, static_cast<int>(std::lround(tet * 7.0 / 12.0)));
        const int span = params_.spanOctaves * tet;
        for (int i = 0; i < kBands; ++i) {
            auto& b = bands_[i];
            const int step = (i * stride) % std::max(1, span);
            const double ratio = std::pow(2.0, static_cast<double>(step) / tet);
            b.nominal = f0_ * ratio * std::pow(2.0, b.trimCents / 1200.0);
            b.harmonic = std::max(1, static_cast<int>(std::lround(f0_ * ratio / fRef_)));
            b.target = fRef_ * b.harmonic;
        }
    }

    void applyDriftParams() noexcept {
        BandDriftParams c; // cycle detune: vintage-like character scaled by cents
        c.slow = 2.5; c.wander = 1.5; c.jitter = 0.25; c.spread = 2.0;
        BandDriftParams k; // chaos
        switch (params_.type) {
            case ChaosType::Flicker: k.slow = 12; k.wander = 0; k.jitter = 0; break;
            case ChaosType::Wander:  k.slow = 0;  k.wander = 10; k.jitter = 0; break;
            case ChaosType::Jitter:  k.slow = 0;  k.wander = 0; k.jitter = 4; break;
            case ChaosType::Blend:   k.slow = 8;  k.wander = 6; k.jitter = 1.2; break;
        }
        k.spread = 0.0;
        for (auto& b : bands_) {
            c.depth = b.cyc.params().depth; k.depth = b.chaos.params().depth; // depths are smoothed in controlTick
            b.cyc.setParams(c); b.chaos.setParams(k);
        }
    }

    double driftScale() const noexcept {
        switch (params_.mode) { case ChaosMode::Locked: return 0.25; case ChaosMode::Drifting: return 1.0; default: return 3.0; }
    }
    double loopScale() const noexcept {
        switch (params_.mode) { case ChaosMode::Locked: return 1.0; case ChaosMode::Drifting: return 1.0; default: return 0.2; }
    }

    void latch() noexcept {
        // Latch what is currently audible so freezing mid-ramp is click-free.
        latchedCoh_ = coherenceSmooth_ + (latchedCoh_ - coherenceSmooth_) * fz_;
        for (auto& b : bands_) b.latched = { b.usedFreq, b.usedFold, b.usedPhaseOff, b.usedLevel };
    }

    void controlTick() noexcept {
        // Smooth discontinuous controls (30 ms) so parameter changes never step phase or depth.
        const double sa = 1.0 - std::exp(-dt_ / 0.030);
        cycDepthS_   += sa * (std::max(0.0, params_.cycleDetuneCents) / 4.0 - cycDepthS_);
        chaosAmtS_   += sa * (std::clamp(params_.chaosAmount, 0.0, 1.0) - chaosAmtS_);
        driftScaleS_ += sa * (driftScale() - driftScaleS_);
        loopScaleS_  += sa * (loopScale() - loopScaleS_);
        foldS_       += sa * (std::clamp(params_.foldAmount, 0.0, 1.0) - foldS_);
        for (auto& b : bands_) { b.cyc.setDepth(cycDepthS_); b.chaos.setDepth(chaosAmtS_ * driftScaleS_); }
        const double speed = 0.05 * std::pow(80.0, std::clamp(params_.cycleSpeed, 0.0, 1.0));
        const double ticks = BandDrift::kControlRate * dt_;
        const double G = std::clamp(params_.gravity, 0.0, 1.0);
        const double P = std::clamp(params_.phaseFieldLock, 0.0, 1.0);
        const double coh = params_.coherenceMult;
        const double kBase = 60.0; // rad/s at gravity 1, coherence x1
        const uint64_t ref = master_.getRawPhase();

        // advance drift (frozen output ignores it via the crossfade; model keeps running)
        for (auto& b : bands_) {
            b.cycCents = b.cyc.advance(ticks * speed);
            b.chaosCents = b.chaos.advance(ticks * (params_.type == ChaosType::Jitter ? 1.0 : 1.0));
        }

        // phase of each band relative to its harmonic reference
        std::array<double, kGroups> cx{}, cy{}; std::array<int, kGroups> cn{};
        double ax = 0.0, ay = 0.0; int an = 0;
        for (int i = 0; i < kBands; ++i) {
            auto& b = bands_[i];
            const uint64_t d = b.acc.getRawPhase() - static_cast<uint64_t>(b.harmonic) * ref;
            b.theta = static_cast<double>(static_cast<int64_t>(d)) * (1.0 / 18446744073709551616.0);
            if (!b.on) continue;
            const double c = std::cos(6.283185307179586 * b.theta), s = std::sin(6.283185307179586 * b.theta);
            const int g = groupOf(i);
            cx[g] += c; cy[g] += s; ++cn[g]; ax += c; ay += s; ++an;
        }
        onCount_ = an;
        coherence_ = an ? std::sqrt(ax * ax + ay * ay) / an : 0.0;
        // fast attack (no overshoot when bands lock), slow release (no pumping while bands beat)
        coherenceSmooth_ += (coherence_ > coherenceSmooth_ ? 0.05 : 0.002) * (coherence_ - coherenceSmooth_);

        for (int i = 0; i < kBands; ++i) {
            auto& b = bands_[i];
            const int g = groupOf(i);
            const double gc = std::max(0.0, params_.groupCoherence[g]);
            const double kG = G * kBase * coh * gc * loopScaleS_;
            const double kP = P * kBase * coh * gc * loopScaleS_;
            const double aG = 1.0 - std::exp(-kG * dt_);
            const double aP = 1.0 - std::exp(-kP * dt_);

            double pull = -aG * b.theta;                       // cycles moved this tick
            if (cn[g] > 0 && aP > 0.0) {
                const double R = std::sqrt(cx[g] * cx[g] + cy[g] * cy[g]) / cn[g];
                const double psi = std::atan2(cy[g], cx[g]) / 6.283185307179586;
                pull += aP * R * wrap01(psi - b.theta);
            }
            double base = b.nominal * std::pow(b.target / b.nominal, G);
            base *= std::pow(2.0, (b.cycCents + b.chaosCents) / 1200.0);
            double df = pull / dt_;
            df = std::clamp(df, -0.3 * base, 0.3 * base);
            b.df = df;

            b.live.freq = std::clamp(base + df, 0.01, 0.499 * sampleRate_);
            b.phaseS += 0.05 * (b.chaosCents * 0.01 * chaosAmtS_ - b.phaseS); // slew: type changes cannot step phase
            b.live.phaseOff = b.phaseS;
            b.live.fold = std::max(0.0, foldS_ * 0.35 * (1.0 + 0.05 * b.chaosCents));
            double lvl = b.on ? b.level : 0.0;
            const double nyq = 0.45 * sampleRate_;
            if (b.live.freq > nyq) lvl *= std::clamp((0.499 * sampleRate_ - b.live.freq) / (0.499 * sampleRate_ - nyq), 0.0, 1.0);
            b.live.level = lvl;

            const double m = fz_;
            b.usedFreq     = b.live.freq     + (b.latched.freq     - b.live.freq)     * m;
            b.usedFold     = b.live.fold     + (b.latched.fold     - b.live.fold)     * m;
            b.usedPhaseOff = b.live.phaseOff + (b.latched.phaseOff - b.live.phaseOff) * m;
            b.usedLevel    = b.live.level    + (b.latched.level    - b.live.level)    * m;
            b.acc.setFrequency(b.usedFreq);
        }
    }

    Params params_;
    double sampleRate_ = 48000.0, dt_ = 32.0 / 48000.0, f0_ = 55.0, fRef_ = 13.75;
    double fz_ = 0.0, fzStep_ = 0.001, ampCoef_ = 0.01;
    double cycDepthS_ = 1.5, chaosAmtS_ = 0.0, driftScaleS_ = 1.0, loopScaleS_ = 1.0, foldS_ = 0.0, levelS_ = 0.6;
    double coherence_ = 0.0, coherenceSmooth_ = 0.0, latchedCoh_ = 0.0;
    int stride_ = 32, ctrlCounter_ = 0, onCount_ = kBands;
    PhaseAccumulator master_;
    std::array<Band, kBands> bands_{};
};

} // namespace cosmic::dsp
