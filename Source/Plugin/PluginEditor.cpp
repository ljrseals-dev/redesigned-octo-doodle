#include "PluginEditor.h"

#if COSMIC_HAS_JUCE

namespace {
    const juce::Colour kBg      { 0xff0a0a12 };
    const juce::Colour kPanel   { 0xff12121e };
    const juce::Colour kViolet  { 0xff9b59ff };
    const juce::Colour kMagenta { 0xffff2d95 };
    const juce::Colour kCyan    { 0xff00e5ff };
    const juce::Colour kText    { 0xffe0e0f0 };
}

CosmicSynthEditor::CosmicSynthEditor (CosmicSynthProcessor& p)
    : AudioProcessorEditor (p), processor_ (p)
{
    setSize (720, 420);
    setResizable (false, false);

    auto styleSlider = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour (juce::Slider::rotarySliderFillColourId, kViolet);
        s.setColour (juce::Slider::thumbColourId, kCyan);
        s.setColour (juce::Slider::textBoxTextColourId, kText);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    };

    styleSlider (freqSlider_);
    styleSlider (fineSlider_);
    styleSlider (levelSlider_);
    styleSlider (driftSlider_);
    styleSlider (nonlinSlider_);
    styleSlider (quadSlider_);
    styleSlider (resoFreqSlider_);
    styleSlider (resoDampSlider_);
    styleSlider (outSlider_);

    waveBox_.addItemList ({ "Sine", "Triangle", "Saw", "Pulse" }, 1);
    waveBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    waveBox_.setColour (juce::ComboBox::textColourId, kText);
    waveBox_.setColour (juce::ComboBox::outlineColourId, kViolet);

    pllButton_.setButtonText ("PLL");
    pllButton_.setColour (juce::ToggleButton::textColourId, kText);
    pllButton_.setColour (juce::ToggleButton::tickColourId, kCyan);

    titleLabel_.setText ("COSMIC SYNTHESIZER ENGINE", juce::dontSendNotification);
    titleLabel_.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel_.setColour (juce::Label::textColourId, kMagenta);
    titleLabel_.setJustificationType (juce::Justification::centred);

    phaseLabel_.setText ("PHASE", juce::dontSendNotification);
    phaseLabel_.setColour (juce::Label::textColourId, kCyan);
    phaseLabel_.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (freqSlider_);
    addAndMakeVisible (fineSlider_);
    addAndMakeVisible (levelSlider_);
    addAndMakeVisible (driftSlider_);
    addAndMakeVisible (nonlinSlider_);
    addAndMakeVisible (quadSlider_);
    addAndMakeVisible (resoFreqSlider_);
    addAndMakeVisible (resoDampSlider_);
    addAndMakeVisible (outSlider_);
    addAndMakeVisible (waveBox_);
    addAndMakeVisible (pllButton_);
    addAndMakeVisible (titleLabel_);
    addAndMakeVisible (phaseLabel_);

    // Attachments – will be connected once parameters are live
    // (Generic fallback if full APVTS not used)
    startTimerHz (30);
}

void CosmicSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Header bar
    g.setColour (kPanel);
    g.fillRect (0, 0, getWidth(), 48);

    // Scope background
    auto scopeArea = juce::Rectangle<int> (20, 60, 280, 120);
    g.setColour (kPanel);
    g.fillRoundedRectangle (scopeArea.toFloat(), 6.f);
    g.setColour (kViolet.withAlpha (0.4f));
    g.drawRoundedRectangle (scopeArea.toFloat(), 6.f, 1.5f);

    // Draw simple waveform / phase trace
    g.setColour (kCyan);
    juce::Path path;
    const float midY = scopeArea.getCentreY();
    const float scaleY = scopeArea.getHeight() * 0.4f;
    for (int i = 0; i < kVizSize; ++i)
    {
        float x = scopeArea.getX() + (float) i / (kVizSize - 1) * scopeArea.getWidth();
        float y = midY - vizBuffer_[i] * scaleY;
        if (i == 0) path.startNewSubPath (x, y);
        else        path.lineTo (x, y);
    }
    g.strokePath (path, juce::PathStrokeType (1.5f));

    // Footer accent line
    g.setColour (kMagenta);
    g.fillRect (0, getHeight() - 3, getWidth(), 3);
}

void CosmicSynthEditor::resized()
{
    titleLabel_.setBounds (0, 8, getWidth(), 32);

    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (50);

    auto scope = area.removeFromLeft (300);
    phaseLabel_.setBounds (scope.removeFromTop (20));
    // scope used in paint

    area.removeFromLeft (20);
    auto knobs = area;

    const int knobW = 80;
    const int knobH = 90;
    auto row1 = knobs.removeFromTop (knobH);
    freqSlider_.setBounds  (row1.removeFromLeft (knobW));
    fineSlider_.setBounds  (row1.removeFromLeft (knobW));
    levelSlider_.setBounds (row1.removeFromLeft (knobW));
    waveBox_.setBounds     (row1.removeFromLeft (100).reduced (0, 30));

    auto row2 = knobs.removeFromTop (knobH);
    driftSlider_.setBounds   (row2.removeFromLeft (knobW));
    nonlinSlider_.setBounds  (row2.removeFromLeft (knobW));
    quadSlider_.setBounds    (row2.removeFromLeft (knobW));
    pllButton_.setBounds     (row2.removeFromLeft (80).reduced (0, 30));

    auto row3 = knobs.removeFromTop (knobH);
    resoFreqSlider_.setBounds (row3.removeFromLeft (knobW));
    resoDampSlider_.setBounds (row3.removeFromLeft (knobW));
    outSlider_.setBounds      (row3.removeFromLeft (knobW));
}

void CosmicSynthEditor::timerCallback()
{
    // Pull a few samples for visualisation (safe – outside audio thread)
    auto& vco = processor_.getVCO();
    for (int i = 0; i < 4; ++i)
    {
        vizBuffer_[vizWrite_] = vco.process(); // note: advances phase for viz only
        vizWrite_ = (vizWrite_ + 1) % kVizSize;
    }
    repaint (20, 60, 280, 120);
}

#endif
