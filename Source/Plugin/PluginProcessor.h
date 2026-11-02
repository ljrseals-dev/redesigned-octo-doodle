#pragma once
/**
 * Cosmic Synthesizer Engine — AudioProcessor
 * Full parameter set, MIDI note handling, versioned state.
 * Compiles with or without JUCE (fallback pure-C++ path).
 */

#if __has_include(<juce_audio_processors/juce_audio_processors.h>)
#  include <juce_audio_processors/juce_audio_processors.h>
#  define COSMIC_HAS_JUCE 1
#else
#  define COSMIC_HAS_JUCE 0
#endif

#include "../DSP/Oscillators/AnalogVCO.h"
#include "../DSP/Oscillators/QuadratureOscillator.h"
#include "../DSP/Filters/Resonator.h"
#include "../DSP/Synchronization/PLLCore.h"
#include "../DSP/AnalogModel/NonlinearTuning.h"
#include "../DSP/AnalogModel/OscillatorDrift.h"

#if COSMIC_HAS_JUCE

class CosmicSynthProcessor : public juce::AudioProcessor
{
public:
    CosmicSynthProcessor();
    ~CosmicSynthProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Public DSP access for editor / tests
    cosmic::dsp::AnalogVCO& getVCO() noexcept { return vco_; }
    cosmic::dsp::QuadratureOscillator& getQuad() noexcept { return quad_; }
    cosmic::dsp::Resonator& getResonator() noexcept { return reso_; }
    cosmic::dsp::PLLCore& getPLL() noexcept { return pll_; }

private:
    void updateParameters();

    cosmic::dsp::AnalogVCO            vco_;
    cosmic::dsp::QuadratureOscillator quad_;
    cosmic::dsp::Resonator            reso_;
    cosmic::dsp::PLLCore              pll_;

    // Parameters
    juce::AudioParameterFloat*  freqParam_       = nullptr;
    juce::AudioParameterFloat*  fineTuneParam_   = nullptr;
    juce::AudioParameterFloat*  levelParam_      = nullptr;
    juce::AudioParameterChoice* waveParam_       = nullptr;
    juce::AudioParameterFloat*  driftParam_      = nullptr;
    juce::AudioParameterFloat*  nonlinParam_     = nullptr;
    juce::AudioParameterFloat*  quadWidthParam_  = nullptr;
    juce::AudioParameterBool*   pllEnableParam_  = nullptr;
    juce::AudioParameterFloat*  resoFreqParam_   = nullptr;
    juce::AudioParameterFloat*  resoDampParam_   = nullptr;
    juce::AudioParameterFloat*  outputParam_     = nullptr;

    double currentSampleRate_ = 44100.0;
    int    currentBlockSize_  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CosmicSynthProcessor)
};

#else // pure C++ fallback

class CosmicSynthProcessor
{
public:
    void prepare (double sr, int /*block*/)
    {
        vco_.setSampleRate (sr);
        quad_.setSampleRate (sr);
        reso_.setSampleRate (sr);
        pll_.setSampleRate (sr);
        currentSampleRate_ = sr;
    }

    float processSample()
    {
        return vco_.process();
    }

    cosmic::dsp::AnalogVCO& getVCO() noexcept { return vco_; }

private:
    cosmic::dsp::AnalogVCO            vco_;
    cosmic::dsp::QuadratureOscillator quad_;
    cosmic::dsp::Resonator            reso_;
    cosmic::dsp::PLLCore              pll_;
    double currentSampleRate_ = 44100.0;
};

#endif
