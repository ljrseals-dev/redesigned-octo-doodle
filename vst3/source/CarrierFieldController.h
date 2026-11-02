#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "CarrierFieldVst3Bridge.h"

namespace cosmic::vst3 {

/** Edit controller: exposes all parameters (generic host editor; no custom view yet). */
class CarrierFieldController : public Steinberg::Vst::EditController
{
public:
    static Steinberg::FUnknown* createInstance (void*) { return static_cast<Steinberg::Vst::IEditController*> (new CarrierFieldController); }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;

    Steinberg::tresult PLUGIN_API getParamStringByValue (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue valueNormalized,
                                                         Steinberg::Vst::String128 string) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getParamValueByString (Steinberg::Vst::ParamID tag, Steinberg::Vst::TChar* string,
                                                         Steinberg::Vst::ParamValue& valueNormalized) SMTG_OVERRIDE;
    Steinberg::Vst::ParamValue PLUGIN_API normalizedParamToPlain (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue valueNormalized) SMTG_OVERRIDE;
    Steinberg::Vst::ParamValue PLUGIN_API plainParamToNormalized (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue plainValue) SMTG_OVERRIDE;
};

} // namespace cosmic::vst3
