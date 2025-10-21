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
// Custom rotary slider with no text box
struct CustomRotarySlider : juce::Slider
{
    CustomRotarySlider()
        : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                       juce::Slider::TextEntryBoxPosition::NoTextBox) {}
};

struct ResponseCurveComponent : juce::Component,
                                juce::AudioProcessorParameter::Listener,
                                juce::Timer
{
    ResponseCurveComponent(OloEQAudioProcessor&);
    ~ResponseCurveComponent();

    // Called when an attached parameter changes
    void parameterValueChanged(int parameterIndex, float newValue) override;

    // Gesture notifications (unused here)
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override
    {
        juce::ignoreUnused(parameterIndex, gestureIsStarting);
    }

    // Timer callback for updating the response curve
    void timerCallback() override;

    // Paint the frequency response curve
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
    // Reference to the processor
    OloEQAudioProcessor& audioProcessor;

    // Sliders
    CustomRotarySlider peakFreqSlider, peakGainSlider, peakQualitySlider;
    CustomRotarySlider lowCutFreqSlider, highCutFreqSlider;
    CustomRotarySlider lowCutSlopeSlider, highCutSlopeSlider;

    // Frequency response component
    ResponseCurveComponent responseCurveComponent;

    // Slider attachments to connect sliders to parameters
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment peakFreqSliderAttachment, peakGainSliderAttachment, peakQualitySliderAttachment;
    Attachment lowCutFreqSliderAttachment, highCutFreqSliderAttachment;
    Attachment lowCutSlopeSliderAttachment, highCutSlopeSliderAttachment;

    // Helper to return all child components
    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OloEQAudioProcessorEditor)
};