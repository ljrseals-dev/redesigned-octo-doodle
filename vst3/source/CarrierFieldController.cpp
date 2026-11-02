#include "CarrierFieldController.h"

#include "pluginterfaces/base/ibstream.h"
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace cosmic::vst3 {

static void ascii16 (const char* s, TChar* out, int cap)
{
    int i = 0;
    for (; s && s[i] && i < cap - 1; ++i)
        out[i] = static_cast<TChar> (static_cast<unsigned char> (s[i]));
    out[i] = 0;
}

static void from16 (const TChar* s, char* out, int cap)
{
    int i = 0;
    for (; s && s[i] && i < cap - 1; ++i)
        out[i] = s[i] < 128 ? static_cast<char> (s[i]) : '?';
    out[i] = 0;
}

tresult PLUGIN_API CarrierFieldController::initialize (FUnknown* context)
{
    const tresult result = EditController::initialize (context);
    if (result != kResultOk)
        return result;

    for (int i = 0; i < kParamCount; ++i)
    {
        const ParamSpec& s = spec (i);
        TChar title[128], units[32];
        ascii16 (s.name, title, 128);
        ascii16 (unitFor (i), units, 32);

        int32 flags = ParameterInfo::kCanAutomate;
        if (s.kind == Kind::Choice)
            flags |= ParameterInfo::kIsList;
        parameters.addParameter (title, units, stepCount (i), defaultNorm (i), flags, static_cast<ParamID> (i));
    }
    return kResultOk;
}

tresult PLUGIN_API CarrierFieldController::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    std::vector<uint8_t> bytes;
    uint8_t chunk[1024];
    for (;;)
    {
        int32 got = 0;
        if (state->read (chunk, static_cast<int32> (sizeof (chunk)), &got) != kResultOk || got <= 0)
            break;
        bytes.insert (bytes.end(), chunk, chunk + got);
        if (bytes.size() > kStateMaxBytes * 4)
            return kResultFalse;
    }
    float plain[kParamCount];
    if (!deserializeState (bytes.data(), bytes.size(), plain))
        return kResultFalse;
    for (int i = 0; i < kParamCount; ++i)
        setParamNormalized (static_cast<ParamID> (i), toNorm (i, plain[i]));
    return kResultOk;
}

IPlugView* PLUGIN_API CarrierFieldController::createView (FIDString)
{
    return nullptr;   // no custom editor yet: the host shows its generic parameter editor
}

tresult PLUGIN_API CarrierFieldController::getParamStringByValue (ParamID tag, ParamValue valueNormalized, String128 string)
{
    if (tag >= static_cast<ParamID> (kParamCount))
        return kResultFalse;
    char text[64];
    formatValue (static_cast<int> (tag), valueNormalized, text, 64);
    ascii16 (text, string, 128);
    return kResultTrue;
}

tresult PLUGIN_API CarrierFieldController::getParamValueByString (ParamID tag, TChar* string, ParamValue& valueNormalized)
{
    if (tag >= static_cast<ParamID> (kParamCount))
        return kResultFalse;
    char text[64];
    from16 (string, text, 64);
    double n = 0.0;
    if (!parseValue (static_cast<int> (tag), text, n))
        return kResultFalse;
    valueNormalized = n;
    return kResultTrue;
}

ParamValue PLUGIN_API CarrierFieldController::normalizedParamToPlain (ParamID tag, ParamValue valueNormalized)
{
    return tag < static_cast<ParamID> (kParamCount) ? toPlain (static_cast<int> (tag), valueNormalized) : valueNormalized;
}

ParamValue PLUGIN_API CarrierFieldController::plainParamToNormalized (ParamID tag, ParamValue plainValue)
{
    return tag < static_cast<ParamID> (kParamCount) ? toNorm (static_cast<int> (tag), plainValue) : plainValue;
}

} // namespace cosmic::vst3
