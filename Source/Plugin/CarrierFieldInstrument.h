#pragma once
/**
 * CarrierFieldInstrument - JUCE-free plugin core.
 *  - One parameter table (ids, ranges, defaults) used by the JUCE shell AND by the tests.
 *  - Controls: plain-value snapshot of every parameter -> CarrierField::Params / band settings.
 *  - Instrument: wraps CarrierField, MIDI note -> fundamental (no sequencing, no note-off logic),
 *    drone on/off with fade, block-size independent rendering.
 * Everything here is allocation-free after construction and safe to call from the audio thread.
 */

#include "../DSP/CarrierField/CarrierField.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace cosmic::plugin {

using cosmic::dsp::CarrierField;
inline constexpr int kBands = CarrierField::kBands;

enum class Kind { Float, Int, Bool, Choice };

struct ParamSpec {
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    float skew;            // 1 = linear (Float only)
    const char* choices;   // '|' separated (Choice only)
};

enum P : int {
    Fundamental, Octave, Tet, Stride, Span, HarmDiv,
    Gravity, PhaseFieldLock, Coherence, GroupLow, GroupMid, GroupHigh,
    CycleCents, CycleSpeed, ChaosMode, ChaosType, ChaosAmount,
    FoldAmount, FoldHz, Level, Freeze, Drone, FollowMidi,
    kGlobalCount
};
inline constexpr int kBandOn = kGlobalCount;
inline constexpr int kBandLevel = kBandOn + kBands;
inline constexpr int kBandTrim = kBandLevel + kBands;
inline constexpr int kParamCount = kBandTrim + kBands;

inline constexpr std::array<ParamSpec, kGlobalCount> kGlobalSpecs {{
    { "fundamental", "Fundamental (Hz)", Kind::Float, 10.f, 2000.f, 55.f, 0.4f, nullptr },
    { "octave",      "Octave Shift",     Kind::Int,  -8.f, 8.f, 0.f, 1.f, nullptr },
    { "tet",         "TET Divisions",    Kind::Int,   1.f, 96.f, 12.f, 1.f, nullptr },
    { "stride",      "Band Step (0=auto)", Kind::Int, 0.f, 96.f, 0.f, 1.f, nullptr },
    { "span",        "Band Span (oct)",  Kind::Int,   1.f, 10.f, 4.f, 1.f, nullptr },
    { "harmdiv",     "Harmonic Divisor", Kind::Int,   1.f, 16.f, 4.f, 1.f, nullptr },
    { "gravity",     "Master Gravity",   Kind::Float, 0.f, 1.f, 0.5f, 1.f, nullptr },
    { "pfl",         "Phase Field Lock", Kind::Float, 0.f, 1.f, 0.f, 1.f, nullptr },
    { "coherence",   "Harmony Coherence", Kind::Choice, 0.f, 2.f, 0.f, 1.f, "x1|x20|x100" },
    { "grp_low",     "Low Group Coherence",  Kind::Float, 0.f, 2.f, 1.f, 1.f, nullptr },
    { "grp_mid",     "Mid Group Coherence",  Kind::Float, 0.f, 2.f, 1.f, 1.f, nullptr },
    { "grp_high",    "High Group Coherence", Kind::Float, 0.f, 2.f, 1.f, 1.f, nullptr },
    { "cycle_cents", "Cycle Detune (cents)", Kind::Float, 0.f, 50.f, 6.f, 0.6f, nullptr },
    { "cycle_speed", "Cycle Speed",      Kind::Float, 0.f, 1.f, 0.3f, 1.f, nullptr },
    { "chaos_mode",  "Chaos Mode",       Kind::Choice, 0.f, 2.f, 1.f, 1.f, "Locked|Drifting|Unstable" },
    { "chaos_type",  "Chaos Type",       Kind::Choice, 0.f, 3.f, 3.f, 1.f, "Flicker|Wander|Jitter|Blend" },
    { "chaos_amount","Chaos Amount",     Kind::Float, 0.f, 1.f, 0.f, 1.f, nullptr },
    { "fold_amount", "Fold Amount",      Kind::Float, 0.f, 1.f, 0.f, 1.f, nullptr },
    { "fold_hz",     "Fold Rate (Hz)",   Kind::Float, 1.f, 60.f, 17.f, 1.f, nullptr },
    { "level",       "Master Level",     Kind::Float, 0.f, 1.f, 0.6f, 1.f, nullptr },
    { "freeze",      "Freeze EM",        Kind::Bool,  0.f, 1.f, 0.f, 1.f, nullptr },
    { "drone",       "Drone On",         Kind::Bool,  0.f, 1.f, 1.f, 1.f, nullptr },
    { "follow_midi", "Follow MIDI Note", Kind::Bool,  0.f, 1.f, 1.f, 1.f, nullptr },
}};

/** Spec for any parameter index 0..kParamCount-1 (band parameters are generated). Ids/names live in storage. */
struct BandSpecStore {
    char id[3 * kBands][32];
    char name[3 * kBands][48];
    std::array<ParamSpec, 3 * kBands> spec{};
    BandSpecStore() {
        for (int i = 0; i < kBands; ++i) {
            const int n = i + 1;
            std::snprintf(id[i], 32, "b%02d_on", n);            std::snprintf(name[i], 48, "Band %02d On", n);
            std::snprintf(id[kBands + i], 32, "b%02d_level", n); std::snprintf(name[kBands + i], 48, "Band %02d Level", n);
            std::snprintf(id[2 * kBands + i], 32, "b%02d_trim", n); std::snprintf(name[2 * kBands + i], 48, "Band %02d Trim (cents)", n);
            spec[i]              = { id[i], name[i], Kind::Bool, 0.f, 1.f, 1.f, 1.f, nullptr };
            spec[kBands + i]     = { id[kBands + i], name[kBands + i], Kind::Float, 0.f, 1.f, 1.f, 1.f, nullptr };
            spec[2 * kBands + i] = { id[2 * kBands + i], name[2 * kBands + i], Kind::Float, -100.f, 100.f, 0.f, 1.f, nullptr };
        }
    }
};
inline const BandSpecStore& bandSpecs() { static const BandSpecStore s; return s; }

inline const ParamSpec& spec(int index) noexcept {
    return index < kGlobalCount ? kGlobalSpecs[static_cast<size_t>(index)]
                                : bandSpecs().spec[static_cast<size_t>(index - kGlobalCount)];
}

/** Plain-value snapshot of every parameter. */
struct Controls {
    double fundamentalHz = 55.0; int octave = 0, tet = 12, stride = 0, span = 4, harmDiv = 4;
    double gravity = 0.5, pfl = 0.0; int coherenceChoice = 0;
    double groupLow = 1.0, groupMid = 1.0, groupHigh = 1.0;
    double cycleCents = 6.0, cycleSpeed = 0.3; int chaosMode = 1, chaosType = 3; double chaosAmount = 0.0;
    double foldAmount = 0.0, foldHz = 17.0, level = 0.6;
    bool freeze = false, drone = true, followMidi = true;
    std::array<bool, kBands> bandOn{}; std::array<double, kBands> bandLevel{}, bandTrim{};

    Controls() { bandOn.fill(true); bandLevel.fill(1.0); bandTrim.fill(0.0); }

    /** values[] is indexed like the parameter table (what a host/APVTS reads). Out-of-range values are clamped. */
    static Controls fromValues(const float* v) noexcept {
        auto cl = [&](int i) { const auto& s = spec(i); return std::min(s.max, std::max(s.min, std::isfinite(v[i]) ? v[i] : s.def)); };
        Controls c;
        c.fundamentalHz = cl(Fundamental); c.octave = static_cast<int>(std::lround(cl(Octave))); c.tet = static_cast<int>(std::lround(cl(Tet)));
        c.stride = static_cast<int>(std::lround(cl(Stride))); c.span = static_cast<int>(std::lround(cl(Span))); c.harmDiv = static_cast<int>(std::lround(cl(HarmDiv)));
        c.gravity = cl(Gravity); c.pfl = cl(PhaseFieldLock); c.coherenceChoice = static_cast<int>(std::lround(cl(Coherence)));
        c.groupLow = cl(GroupLow); c.groupMid = cl(GroupMid); c.groupHigh = cl(GroupHigh);
        c.cycleCents = cl(CycleCents); c.cycleSpeed = cl(CycleSpeed);
        c.chaosMode = static_cast<int>(std::lround(cl(ChaosMode))); c.chaosType = static_cast<int>(std::lround(cl(ChaosType))); c.chaosAmount = cl(ChaosAmount);
        c.foldAmount = cl(FoldAmount); c.foldHz = cl(FoldHz); c.level = cl(Level);
        c.freeze = cl(Freeze) >= 0.5f; c.drone = cl(Drone) >= 0.5f; c.followMidi = cl(FollowMidi) >= 0.5f;
        for (int i = 0; i < kBands; ++i) { c.bandOn[i] = cl(kBandOn + i) >= 0.5f; c.bandLevel[i] = cl(kBandLevel + i); c.bandTrim[i] = cl(kBandTrim + i); }
        return c;
    }
};

inline void defaultValues(float* v) noexcept { for (int i = 0; i < kParamCount; ++i) v[i] = spec(i).def; }
inline double midiNoteToHz(int note) noexcept { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

class Instrument {
public:
    void prepare(double sampleRate) noexcept {
        cf_.setSampleRate(sampleRate);
        apply(Controls{});
        cf_.reset();
    }
    void setControls(const Controls& c) noexcept { apply(c); }
    void noteOn(int note) noexcept { midiNote_ = std::min(127, std::max(0, note)); haveNote_ = true; }
    void clearNote() noexcept { haveNote_ = false; }
    void render(float* left, float* right, int n) noexcept {
        for (int i = 0; i < n; ++i) { const float x = cf_.process(); left[i] = x; if (right) right[i] = x; }
    }
    const CarrierField& engine() const noexcept { return cf_; }
    CarrierField& engine() noexcept { return cf_; }
    double currentFundamental() const noexcept { return fundamentalNow_; }

private:
    void apply(const Controls& c) noexcept {
        CarrierField::Params p = cf_.params();
        fundamentalNow_ = (c.followMidi && haveNote_) ? midiNoteToHz(midiNote_) : c.fundamentalHz;
        p.fundamentalHz = fundamentalNow_; p.octaveShift = c.octave; p.tet = c.tet; p.stepStride = c.stride;
        p.spanOctaves = c.span; p.harmonicDivisor = c.harmDiv;
        p.gravity = c.gravity; p.phaseFieldLock = c.pfl;
        p.coherenceMult = c.coherenceChoice >= 2 ? 100 : (c.coherenceChoice == 1 ? 20 : 1);
        p.groupCoherence = { c.groupLow, c.groupMid, c.groupHigh };
        p.cycleDetuneCents = c.cycleCents; p.cycleSpeed = c.cycleSpeed;
        p.mode = c.chaosMode >= 2 ? CarrierField::ChaosMode::Unstable : (c.chaosMode == 1 ? CarrierField::ChaosMode::Drifting : CarrierField::ChaosMode::Locked);
        switch (c.chaosType) { case 0: p.type = CarrierField::ChaosType::Flicker; break; case 1: p.type = CarrierField::ChaosType::Wander; break;
                               case 2: p.type = CarrierField::ChaosType::Jitter; break; default: p.type = CarrierField::ChaosType::Blend; break; }
        p.chaosAmount = c.chaosAmount; p.foldAmount = c.foldAmount; p.foldHz = c.foldHz;
        p.masterLevel = c.drone ? c.level : 0.0; p.freeze = c.freeze;
        cf_.setParams(p);
        for (int i = 0; i < kBands; ++i) cf_.setBand(i, c.bandOn[i], c.bandLevel[i], c.bandTrim[i]);
    }

    CarrierField cf_;
    int midiNote_ = 33; bool haveNote_ = false; double fundamentalNow_ = 55.0;
};

} // namespace cosmic::plugin
