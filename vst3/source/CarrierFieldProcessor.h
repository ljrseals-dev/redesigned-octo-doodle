#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "CarrierFieldVst3Bridge.h"
#include <atomic>

namespace cosmic::vst3 {

/** VST3 audio processor. All behaviour lives in Core (tested without the SDK); this class only adapts SDK calls. */
class CarrierFieldProcessor : public Steinberg::Vst::AudioEffect
{
public:
    CarrierFieldProcessor();

    static Steinberg::FUnknown* createInstance (void*) { return static_cast<Steinberg::Vst::IAudioProcessor*> (new CarrierFieldProcessor); }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                                      Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
    Core core_;
    float staged_[kParamCount] {};
    std::atomic<bool> stagedReady_ { false };
};

} // namespace cosmic::vst3
