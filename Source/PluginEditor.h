/*
  ==============================================================================

    PluginEditor.h
    Declares the OloEQAudioProcessorEditor class and the ResponseCurveComponent,
    along with custom sliders and attachments for parameter control.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom rotary slider with optional text box
struct CustomRotarySlider : juce::Slider
{
    CustomRotarySlider(bool showTextBox = false)
        : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                       showTextBox ? juce::Slider::TextEntryBoxPosition::TextBoxBelow
                                   : juce::Slider::TextEntryBoxPosition::NoTextBox)
    {
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setTextBoxIsEditable(false);
    }
};


//==============================================================================
// Frequency response component
struct ResponseCurveComponent : juce::Component,
                                juce::AudioProcessorParameter::Listener,
                                juce::Timer
{
    ResponseCurveComponent(OloEQAudioProcessor&);
    ~ResponseCurveComponent();

    void parameterValueChanged(int parameterIndex, float newValue) override;

    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override
    {
        juce::ignoreUnused(parameterIndex, gestureIsStarting);
    }

    void timerCallback() override;

    void paint(juce::Graphics& g) override;

private:
    OloEQAudioProcessor& audioProcessor;
    juce::Atomic<bool> parametersChanged{ false };

    MonoChain monoChain;
};

//==============================================================================
// Main plugin editor class
class OloEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    OloEQAudioProcessorEditor(OloEQAudioProcessor&);
    ~OloEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    OloEQAudioProcessor& audioProcessor;

    // Sliders
    CustomRotarySlider peakFreqSlider, peakGainSlider, peakQualitySlider;
    CustomRotarySlider lowCutFreqSlider, highCutFreqSlider;
    CustomRotarySlider lowCutSlopeSlider, highCutSlopeSlider;

    // Frequency response component
    ResponseCurveComponent responseCurveComponent;

    // Slider attachments
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment peakFreqSliderAttachment, peakGainSliderAttachment, peakQualitySliderAttachment;
    Attachment lowCutFreqSliderAttachment, highCutFreqSliderAttachment;
    Attachment lowCutSlopeSliderAttachment, highCutSlopeSliderAttachment;

    // Apply look and feel to all sliders (will handle notches and white text)
    void applyDialLookAndFeel();

    // Helper to return all child components
    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OloEQAudioProcessorEditor)
};
