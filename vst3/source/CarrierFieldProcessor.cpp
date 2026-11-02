#include "CarrierFieldProcessor.h"
#include "cids.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace cosmic::vst3 {

CarrierFieldProcessor::CarrierFieldProcessor()
{
    setControllerClass (kCarrierFieldControllerUID);
    defaultValues (staged_);
}

tresult PLUGIN_API CarrierFieldProcessor::initialize (FUnknown* context)
{
    const tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);   // instrument: no audio input
    addEventInput (STR16 ("Event In"), 1);                          // MIDI note-on sets the fundamental
    return kResultOk;
}

tresult PLUGIN_API CarrierFieldProcessor::setBusArrangements (SpeakerArrangement*, int32 numIns,
                                                              SpeakerArrangement* outputs, int32 numOuts)
{
    if (numIns != 0 || numOuts != 1)
        return kResultFalse;
    if (outputs[0] != SpeakerArr::kMono && outputs[0] != SpeakerArr::kStereo)
        return kResultFalse;
    if (auto* bus = FCast<AudioBus> (audioOutputs.at (0)))
    {
        bus->setArrangement (outputs[0]);
        return kResultTrue;
    }
    return kResultFalse;
}

tresult PLUGIN_API CarrierFieldProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
    return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API CarrierFieldProcessor::setupProcessing (ProcessSetup& setup)
{
    core_.prepare (setup.sampleRate);
    stagedReady_.store (false);
    return AudioEffect::setupProcessing (setup);
}

tresult PLUGIN_API CarrierFieldProcessor::setActive (TBool state)
{
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API CarrierFieldProcessor::process (ProcessData& data)
{
    // State restored on the UI thread is handed over here (hosts do not call setState concurrently with process).
    if (stagedReady_.exchange (false, std::memory_order_acquire))
        core_.loadPlain (staged_);

    if (data.inputParameterChanges)
    {
        const int32 numParams = data.inputParameterChanges->getParameterCount();
        for (int32 i = 0; i < numParams; ++i)
        {
            IParamValueQueue* queue = data.inputParameterChanges->getParameterData (i);
            if (!queue)
                continue;
            const ParamID id = queue->getParameterId();
            if (id >= static_cast<ParamID> (kParamCount))
                continue;
            const int32 points = queue->getPointCount();
            for (int32 p = 0; p < points; ++p)
            {
                int32 offset = 0;
                ParamValue value = 0.0;
                if (queue->getPoint (p, offset, value) == kResultTrue)
                    core_.pushParam (offset, static_cast<int> (id), value);
            }
        }
    }

    if (data.inputEvents)
    {
        const int32 numEvents = data.inputEvents->getEventCount();
        for (int32 i = 0; i < numEvents; ++i)
        {
            Event e {};
            if (data.inputEvents->getEvent (i, e) == kResultOk && e.type == Event::kNoteOnEvent && e.noteOn.velocity > 0.f)
                core_.pushNote (e.sampleOffset, e.noteOn.pitch);   // sets the fundamental only; no polyphony, no note-off
        }
    }

    if (data.numOutputs < 1 || data.numSamples <= 0 || data.symbolicSampleSize != kSample32)
    {
        core_.render (nullptr, nullptr, 0);   // still apply queued parameter changes (flush)
        return kResultOk;
    }

    AudioBusBuffers& out = data.outputs[0];
    if (out.numChannels < 1 || !out.channelBuffers32)
        return kResultOk;

    float* left  = out.channelBuffers32[0];
    float* right = out.numChannels > 1 ? out.channelBuffers32[1] : nullptr;
    core_.render (left, right, data.numSamples);
    out.silenceFlags = 0;
    return kResultOk;
}

static bool readAll (IBStream* stream, std::vector<uint8_t>& bytes)
{
    if (!stream)
        return false;
    uint8_t chunk[1024];
    for (;;)
    {
        int32 got = 0;
        if (stream->read (chunk, static_cast<int32> (sizeof (chunk)), &got) != kResultOk || got <= 0)
            break;
        bytes.insert (bytes.end(), chunk, chunk + got);
        if (bytes.size() > kStateMaxBytes * 4)   // refuse absurd streams
            return false;
    }
    return true;
}

tresult PLUGIN_API CarrierFieldProcessor::setState (IBStream* state)
{
    std::vector<uint8_t> bytes;
    if (!readAll (state, bytes))
        return kResultFalse;
    float plain[kParamCount];
    if (!deserializeState (bytes.data(), bytes.size(), plain))
        return kResultFalse;
    std::memcpy (staged_, plain, sizeof (staged_));
    stagedReady_.store (true, std::memory_order_release);
    return kResultOk;
}

tresult PLUGIN_API CarrierFieldProcessor::getState (IBStream* state)
{
    if (!state)
        return kResultFalse;
    uint8_t buf[kStateMaxBytes];
    const size_t n = serializeState (core_.values(), buf, sizeof (buf));
    if (n == 0)
        return kResultFalse;
    int32 written = 0;
    return state->write (buf, static_cast<int32> (n), &written) == kResultOk && written == static_cast<int32> (n) ? kResultOk : kResultFalse;
}

} // namespace cosmic::vst3
