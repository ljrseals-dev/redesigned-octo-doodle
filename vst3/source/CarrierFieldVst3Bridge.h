#pragma once
/**
 * SDK-free core of the VST3 plugin. Everything with logic lives here so it can be unit-tested without
 * the Steinberg SDK (Tests/carrierfield_vst3_bridge_tests.cpp):
 *   - normalized <-> plain conversion, step counts, default values, value <-> string (per parameter)
 *   - binary state format (versioned, validated, forward/backward tolerant)
 *   - Core: sample-accurate parameter/MIDI event handling and rendering around the CarrierField engine.
 * Real-time contract: Core::render and the push functions never allocate, lock or do I/O.
 */
#include "../../Source/Plugin/CarrierFieldInstrument.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace cosmic::vst3 {

using namespace cosmic::plugin;

// ---------------------------------------------------------------- parameter conversion
inline int stepCount(int i) noexcept {
    const ParamSpec& s = spec(i);
    switch (s.kind) { case Kind::Float: return 0; case Kind::Bool: return 1; default: return static_cast<int>(s.max - s.min); }
}
inline double toPlain(int i, double n) noexcept {
    const ParamSpec& s = spec(i);
    n = std::min(1.0, std::max(0.0, std::isfinite(n) ? n : 0.0));
    if (s.kind == Kind::Float) return s.min + (s.max - s.min) * (s.skew == 1.0f ? n : std::pow(n, 1.0 / s.skew));
    return std::round(s.min + n * (s.max - s.min));
}
inline double toNorm(int i, double v) noexcept {
    const ParamSpec& s = spec(i);
    v = std::min<double>(s.max, std::max<double>(s.min, std::isfinite(v) ? v : s.def));
    const double n = (v - s.min) / (s.max - s.min);
    if (s.kind == Kind::Float && s.skew != 1.0f) return std::pow(n, s.skew);
    return n;
}
inline double defaultNorm(int i) noexcept { return toNorm(i, spec(i).def); }

inline const char* unitFor(int i) noexcept {
    const std::string_view id = spec(i).id;
    if (id == "fundamental" || id == "fold_hz") return "Hz";
    if (id == "cycle_cents" || id.substr(id.size() >= 5 ? id.size() - 5 : 0) == "_trim") return "ct";
    return "";
}

inline const char* choiceToken(const char* list, int index, char* buf, int cap) noexcept {
    int k = 0; const char* p = list;
    while (k < index && *p) { if (*p++ == '|') ++k; }
    int n = 0; while (p[n] && p[n] != '|' && n < cap - 1) { buf[n] = p[n]; ++n; }
    buf[n] = 0; return buf;
}

/** Human-readable text for a normalized value (ASCII). Returns characters written. */
inline int formatValue(int i, double norm, char* out, int cap) noexcept {
    const ParamSpec& s = spec(i); const double v = toPlain(i, norm);
    switch (s.kind) {
        case Kind::Bool: return std::snprintf(out, cap, "%s", v >= 0.5 ? "On" : "Off");
        case Kind::Choice: { char t[32]; choiceToken(s.choices, static_cast<int>(v), t, 32); return std::snprintf(out, cap, "%s", t); }
        case Kind::Int: return std::snprintf(out, cap, "%d", static_cast<int>(v));
        default: return std::snprintf(out, cap, (s.max - s.min) >= 100.f ? "%.1f" : "%.3f", v);
    }
}
/** Parse user text to a normalized value. Accepts numbers (optionally with unit), On/Off, choice names. */
inline bool parseValue(int i, const char* text, double& norm) noexcept {
    if (!text) return false;
    const ParamSpec& s = spec(i);
    if (s.kind == Kind::Bool) {
        if (!strncmp(text, "On", 2) || !strncmp(text, "on", 2) || text[0] == '1') { norm = 1.0; return true; }
        if (!strncmp(text, "Off", 3) || !strncmp(text, "off", 3) || text[0] == '0') { norm = 0.0; return true; }
        return false;
    }
    if (s.kind == Kind::Choice) {
        for (int k = 0; k <= static_cast<int>(s.max); ++k) { char t[32]; choiceToken(s.choices, k, t, 32); if (!strcmp(t, text)) { norm = toNorm(i, k); return true; } }
    }
    char* end = nullptr; const double v = std::strtod(text, &end);
    if (end == text || !std::isfinite(v)) return false;
    norm = toNorm(i, v); return true;
}

// ---------------------------------------------------------------- state
inline constexpr uint32_t kStateMagic = 0x31764643u; // bytes 'C','F','v','1'
inline constexpr uint32_t kStateVersion = 1;
inline constexpr size_t kStateHeader = 12;
inline constexpr size_t kStateMaxBytes = kStateHeader + 4 * static_cast<size_t>(kParamCount);

/** Writes magic, version, count, then plain float values (little-endian hosts). Returns bytes written, 0 if cap too small. */
inline size_t serializeState(const float* plain, uint8_t* out, size_t cap) noexcept {
    if (cap < kStateMaxBytes) return 0;
    const uint32_t hdr[3] = { kStateMagic, kStateVersion, static_cast<uint32_t>(kParamCount) };
    std::memcpy(out, hdr, 12); std::memcpy(out + 12, plain, 4 * static_cast<size_t>(kParamCount));
    return kStateMaxBytes;
}
/** Validates and loads. Unknown/missing parameters take defaults; values are clamped; NaN -> default. Returns false (output untouched) on bad input. */
inline bool deserializeState(const uint8_t* in, size_t size, float* plain) noexcept {
    if (!in || size < kStateHeader) return false;
    uint32_t hdr[3]; std::memcpy(hdr, in, 12);
    if (hdr[0] != kStateMagic || hdr[1] == 0 || hdr[1] > kStateVersion) return false;
    const size_t count = hdr[2];
    if (count > 4096 || size < kStateHeader + 4 * count) return false;
    float tmp[kParamCount]; defaultValues(tmp);
    const size_t use = std::min<size_t>(count, kParamCount);
    for (size_t i = 0; i < use; ++i) {
        float v; std::memcpy(&v, in + kStateHeader + 4 * i, 4);
        const ParamSpec& s = spec(static_cast<int>(i));
        tmp[i] = std::isfinite(v) ? std::min(s.max, std::max(s.min, v)) : s.def;
    }
    std::memcpy(plain, tmp, sizeof(tmp)); return true;
}

// ---------------------------------------------------------------- core
class Core {
public:
    static constexpr int kMaxEvents = 1024;

    Core() { defaultValues(values_); }

    void prepare(double sampleRate) noexcept {
        inst_.prepare(sampleRate); count_ = 0; dirty_ = true;
        inst_.setControls(Controls::fromValues(values_));
    }
    /** Replace all parameter values (plain). Not for the audio thread unless the host serialises it with process(). */
    void loadPlain(const float* plain) noexcept { std::memcpy(values_, plain, sizeof(values_)); dirty_ = true; }
    const float* values() const noexcept { return values_; }

    void pushParam(int offset, int index, double normalized) noexcept {
        if (index < 0 || index >= kParamCount) return;
        if (count_ >= kMaxEvents) { values_[index] = static_cast<float>(toPlain(index, normalized)); dirty_ = true; return; }
        ev_[count_++] = { offset, index, static_cast<float>(toPlain(index, normalized)), -1 };
    }
    void pushNote(int offset, int note) noexcept {
        if (count_ >= kMaxEvents) { inst_.noteOn(note); dirty_ = true; return; }
        ev_[count_++] = { offset, -1, 0.f, note };
    }

    /** Renders n samples (left, and right if non-null), applying queued events at their sample offsets. n == 0 just applies events. */
    void render(float* left, float* right, int n) noexcept {
        std::stable_sort(ev_.begin(), ev_.begin() + count_, [](const Ev& a, const Ev& b) { return a.offset < b.offset; });
        int pos = 0;
        for (int k = 0; k < count_; ++k) {
            const int off = std::min(n, std::max(0, ev_[k].offset));
            if (off > pos) { flush(); inst_.render(left + pos, right ? right + pos : nullptr, off - pos); pos = off; }
            if (ev_[k].index >= 0) values_[ev_[k].index] = ev_[k].value; else inst_.noteOn(ev_[k].note);
            dirty_ = true;
        }
        count_ = 0; flush();
        if (n > pos) inst_.render(left + pos, right ? right + pos : nullptr, n - pos);
    }
    const Instrument& instrument() const noexcept { return inst_; }

private:
    struct Ev { int offset; int index; float value; int note; };
    void flush() noexcept { if (dirty_) { inst_.setControls(Controls::fromValues(values_)); dirty_ = false; } }

    Instrument inst_;
    float values_[kParamCount];
    std::array<Ev, kMaxEvents> ev_{};
    int count_ = 0;
    bool dirty_ = true;
};

} // namespace cosmic::vst3
