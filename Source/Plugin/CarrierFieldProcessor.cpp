#include "CarrierFieldProcessor.h"

using namespace cosmic::plugin;

juce::AudioProcessorValueTreeState::ParameterLayout CarrierFieldProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kParamCount; ++i)
    {
        const ParamSpec& s = spec (i);
        const juce::ParameterID pid { s.id, 1 };
        const juce::String name { s.name };

        switch (s.kind)
        {
            case Kind::Float:
            {
                const juce::NormalisableRange<float> range (s.min, s.max, 0.0f, s.skew);
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, name, range, s.def));
                break;
            }
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (pid, name, static_cast<int> (s.min),
                                                                        static_cast<int> (s.max), static_cast<int> (s.def)));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, s.def >= 0.5f));
                break;
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name,
                                juce::StringArray::fromTokens (s.choices, "|", ""), static_cast<int> (s.def)));
                break;
        }
    }
    return layout;
}

CarrierFieldProcessor::CarrierFieldProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CarrierFieldState", createLayout())
{
    for (int i = 0; i < kParamCount; ++i)
        raw_[static_cast<size_t> (i)] = apvts.getRawParameterValue (spec (i).id);
}

void CarrierFieldProcessor::prepareToPlay (double sampleRate, int)
{
    instrument_.prepare (sampleRate);
}

bool CarrierFieldProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void CarrierFieldProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            instrument_.noteOn (msg.getNoteNumber());   // sets the fundamental; no note-off, no sequencing
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            instrument_.clearNote();
    }

    float values[kParamCount];
    for (int i = 0; i < kParamCount; ++i)
        values[i] = raw_[static_cast<size_t> (i)]->load();
    instrument_.setControls (Controls::fromValues (values));

    const int numSamples = buffer.getNumSamples();
    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    instrument_.render (left, right, numSamples);
}

juce::AudioProcessorEditor* CarrierFieldProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void CarrierFieldProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CarrierFieldProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CarrierFieldProcessor();
}
