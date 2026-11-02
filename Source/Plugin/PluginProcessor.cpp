#include "PluginProcessor.h"

#if COSMIC_HAS_JUCE
#include "PluginEditor.h"

//==============================================================================
CosmicSynthProcessor::CosmicSynthProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (freqParam_ = new juce::AudioParameterFloat (
        { "freq", 1 }, "Frequency",
        juce::NormalisableRange<float> (20.f, 20000.f, 0.01f, 0.25f), 440.f));

    addParameter (fineTuneParam_ = new juce::AudioParameterFloat (
        { "fine", 1 }, "Fine Tune",
        juce::NormalisableRange<float> (-100.f, 100.f, 0.1f), 0.f));

    addParameter (levelParam_ = new juce::AudioParameterFloat (
        { "level", 1 }, "Osc Level", 0.f, 1.f, 0.7f));

    addParameter (waveParam_ = new juce::AudioParameterChoice (
        { "wave", 1 }, "Waveform",
        juce::StringArray { "Sine", "Triangle", "Saw", "Pulse" }, 0));

    addParameter (driftParam_ = new juce::AudioParameterFloat (
        { "drift", 1 }, "Drift", 0.f, 1.f, 0.f));

    addParameter (nonlinParam_ = new juce::AudioParameterFloat (
        { "nonlin", 1 }, "Nonlinearity", 0.f, 1.f, 0.f));

    addParameter (quadWidthParam_ = new juce::AudioParameterFloat (
        { "quad", 1 }, "Quad Width", 0.f, 1.f, 0.f));

    addParameter (pllEnableParam_ = new juce::AudioParameterBool (
        { "pll", 1 }, "PLL Track", false));

    addParameter (resoFreqParam_ = new juce::AudioParameterFloat (
        { "resof", 1 }, "Reso Freq",
        juce::NormalisableRange<float> (20.f, 12000.f, 0.1f, 0.3f), 1000.f));

    addParameter (resoDampParam_ = new juce::AudioParameterFloat (
        { "resod", 1 }, "Reso Damp", 0.1f, 50.f, 8.f));

    addParameter (outputParam_ = new juce::AudioParameterFloat (
        { "out", 1 }, "Output", 0.f, 1.f, 0.8f));
}

void CosmicSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate_ = sampleRate;
    currentBlockSize_  = samplesPerBlock;

    vco_.setSampleRate (sampleRate);
    quad_.setSampleRate (sampleRate);
    reso_.setSampleRate (sampleRate);
    pll_.setSampleRate (sampleRate);

    vco_.reset();
    quad_.reset();
    reso_.reset();
    pll_.reset();

    updateParameters();
}

void CosmicSynthProcessor::releaseResources() {}

bool CosmicSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void CosmicSynthProcessor::updateParameters()
{
    const float fineCents = *fineTuneParam_;
    const float baseHz    = *freqParam_;
    const float tunedHz   = baseHz * std::pow (2.f, fineCents / 1200.f);

    vco_.setFrequency (tunedHz);
    vco_.setWaveform (static_cast<cosmic::dsp::Waveform> (waveParam_->getIndex()));

    // Drift
    const float driftAmt = *driftParam_;
    vco_.drift().setEnabled (driftAmt > 0.001f);
    vco_.drift().setDepth (driftAmt * 3.0, driftAmt * 0.05, driftAmt * 0.02);

    // Nonlinear tuning
    const float nl = *nonlinParam_;
    vco_.tuning().setEnabled (nl > 0.001f);
    vco_.tuning().setCoefficients (nl * 5.0, nl * 2.0, nl * 0.5);

    // Resonator
    reso_.setFrequency (*resoFreqParam_);
    reso_.setDamping (*resoDampParam_);

    // PLL
    pll_.setEnabled (*pllEnableParam_);
    pll_.setNominalFrequency (tunedHz);
}

void CosmicSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChans   = buffer.getNumChannels();

    buffer.clear();

    // MIDI note → frequency
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const float f = 440.f * std::pow (2.f, (msg.getNoteNumber() - 69) / 12.f);
            *freqParam_ = f;
        }
    }

    updateParameters();

    auto* left  = buffer.getWritePointer (0);
    auto* right = numChans > 1 ? buffer.getWritePointer (1) : left;

    const float oscLevel = *levelParam_;
    const float outLevel = *outputParam_;
    const float qWidth   = *quadWidthParam_;

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = vco_.process() * oscLevel;

        // Optional light resonator coloration
        mono = reso_.process (mono);

        // Quadrature stereo spread
        auto [I, Q] = quad_.process();
        float L = mono * (1.f - qWidth * 0.5f) + Q * qWidth * 0.3f;
        float R = mono * (1.f - qWidth * 0.5f) + I * qWidth * 0.3f;

        left[i]  = L * outLevel;
        right[i] = R * outLevel;
    }
}

//==============================================================================
juce::AudioProcessorEditor* CosmicSynthProcessor::createEditor()
{
    return new CosmicSynthEditor (*this);
}

//==============================================================================
void CosmicSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, true);
    mos.writeInt (1); // version
    mos.writeFloat (*freqParam_);
    mos.writeFloat (*fineTuneParam_);
    mos.writeFloat (*levelParam_);
    mos.writeInt   (waveParam_->getIndex());
    mos.writeFloat (*driftParam_);
    mos.writeFloat (*nonlinParam_);
    mos.writeFloat (*quadWidthParam_);
    mos.writeBool  (*pllEnableParam_);
    mos.writeFloat (*resoFreqParam_);
    mos.writeFloat (*resoDampParam_);
    mos.writeFloat (*outputParam_);
}

void CosmicSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis (data, static_cast<size_t> (sizeInBytes), false);
    const int ver = mis.readInt();
    if (ver >= 1)
    {
        *freqParam_      = mis.readFloat();
        *fineTuneParam_  = mis.readFloat();
        *levelParam_     = mis.readFloat();
        waveParam_->operator= (mis.readInt());
        *driftParam_     = mis.readFloat();
        *nonlinParam_    = mis.readFloat();
        *quadWidthParam_ = mis.readFloat();
        *pllEnableParam_ = mis.readBool();
        *resoFreqParam_  = mis.readFloat();
        *resoDampParam_  = mis.readFloat();
        *outputParam_    = mis.readFloat();
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CosmicSynthProcessor();
}

#endif // COSMIC_HAS_JUCE
