#pragma once
/**
 * CarrierField plugin shell (JUCE). All DSP and parameter semantics live in the JUCE-free
 * CarrierFieldInstrument.h, which is unit-tested without JUCE. This file only:
 *   - builds the APVTS layout from the shared parameter table,
 *   - reads the parameters once per block into a Controls snapshot,
 *   - forwards MIDI note-ons to the instrument and renders into the output buffer.
 */
#include <juce_audio_processors/juce_audio_processors.h>
#include "CarrierFieldInstrument.h"

class CarrierFieldProcessor : public juce::AudioProcessor
{
public:
    CarrierFieldProcessor();
    ~CarrierFieldProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CarrierField"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    cosmic::plugin::Instrument instrument_;
    std::array<std::atomic<float>*, cosmic::plugin::kParamCount> raw_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CarrierFieldProcessor)
};
