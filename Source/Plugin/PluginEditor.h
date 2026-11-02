#pragma once
/**
 * Cosmic Synthesizer Engine — Editor
 * Cosmic dark theme with violet / magenta / cyan accents.
 * Oscilloscope-style phase display, frequency readout, meters.
 */

#if __has_include(<juce_audio_processors/juce_audio_processors.h>)
#  include <juce_audio_processors/juce_audio_processors.h>
#  include "PluginProcessor.h"
#  define COSMIC_HAS_JUCE 1
#else
#  define COSMIC_HAS_JUCE 0
#endif

#if COSMIC_HAS_JUCE

class CosmicSynthEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit CosmicSynthEditor (CosmicSynthProcessor&);
    ~CosmicSynthEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CosmicSynthProcessor& processor_;

    // Controls
    juce::Slider freqSlider_, fineSlider_, levelSlider_, driftSlider_;
    juce::Slider nonlinSlider_, quadSlider_, resoFreqSlider_, resoDampSlider_, outSlider_;
    juce::ComboBox waveBox_;
    juce::ToggleButton pllButton_;

    juce::Label freqLabel_, waveLabel_, titleLabel_, phaseLabel_;

    using Attachment = juce::SliderParameterAttachment;
    std::unique_ptr<Attachment> freqAtt_, fineAtt_, levelAtt_, driftAtt_;
    std::unique_ptr<Attachment> nonlinAtt_, quadAtt_, resoFAtt_, resoDAtt_, outAtt_;
    std::unique_ptr<juce::ComboBoxParameterAttachment> waveAtt_;
    std::unique_ptr<juce::ButtonParameterAttachment> pllAtt_;

    // Simple phase / waveform visualiser buffer
    static constexpr int kVizSize = 128;
    std::array<float, kVizSize> vizBuffer_ {};
    int vizWrite_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CosmicSynthEditor)
};

#endif
