/*
  ==============================================================================

    PluginEditor.cpp
    Implements the OloEQAudioProcessorEditor and ResponseCurveComponent,
    including UI layout, slider attachments, and frequency response painting.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ResponseCurveComponent::ResponseCurveComponent(OloEQAudioProcessor& p) : audioProcessor(p)
{
    const auto& params = audioProcessor.getParameters();
    for (auto* param : params)
        param->addListener(this);

    startTimerHz(60); // repaint at 60Hz
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for (auto* param : params)
        param->removeListener(this);
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    juce::ignoreUnused(parameterIndex, newValue);
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
    if (parametersChanged.compareAndSetBool(false, true))
    {
        // Update the monochain
        auto chainSettings = getChainSettings(audioProcessor.apvts);
        
        auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

        auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
        auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());

        updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
        updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);

        repaint();
    }
}

namespace
{
    /** Maps a normalized 0–1 value to a logarithmic frequency range. */
    float mapToLog10(float normalizedValue, float minFreq, float maxFreq)
    {
        auto logMin = std::log10(minFreq);
        auto logMax = std::log10(maxFreq);
        auto logValue = logMin + normalizedValue * (logMax - logMin);
        return std::pow(10.0f, logValue);
    }
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);

    auto responseArea = getLocalBounds();
    auto w = responseArea.getWidth();

    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    auto sampleRate = audioProcessor.getSampleRate();

    std::vector<float> mags(w, 1.0f);

    for (int i = 0; i < w; ++i)
    {
        float mag = 1.0f;
        auto freq = mapToLog10(static_cast<float>(i) / static_cast<float>(w), 20.0f, 20000.0f);

        if (!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= static_cast<float>(peak.coefficients->getMagnitudeForFrequency(freq, sampleRate));

        if (!lowCut.isBypassed<0>())
            mag *= static_cast<float>(lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate));
        if (!lowCut.isBypassed<1>())
            mag *= static_cast<float>(lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate));
        if (!lowCut.isBypassed<2>())
            mag *= static_cast<float>(lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate));
        if (!lowCut.isBypassed<3>())
            mag *= static_cast<float>(lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate));

        if (!highCut.isBypassed<0>())
            mag *= static_cast<float>(highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate));
        if (!highCut.isBypassed<1>())
            mag *= static_cast<float>(highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate));
        if (!highCut.isBypassed<2>())
            mag *= static_cast<float>(highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate));
        if (!highCut.isBypassed<3>())
            mag *= static_cast<float>(highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate));

        mags[i] = juce::Decibels::gainToDecibels(mag);
    }

    juce::Path responseCurve;

    const float outputMin = static_cast<float>(responseArea.getBottom());
    const float outputMax = static_cast<float>(responseArea.getY());

    auto map = [outputMin, outputMax](float input)
    {
        return juce::jmap(input, -24.0f, 24.0f, outputMin, outputMax);
    };

    responseCurve.startNewSubPath(
        static_cast<float>(responseArea.getX()),
        map(mags.front())
    );

    const float startX = static_cast<float>(responseArea.getX());
    for (size_t i = 1; i < mags.size(); ++i)
        responseCurve.lineTo(startX + static_cast<float>(i), map(mags[i]));

    g.setColour(juce::Colours::green);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);

    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.f));

}


//==============================================================================
OloEQAudioProcessorEditor::OloEQAudioProcessorEditor (OloEQAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    responseCurveComponent(audioProcessor),
    peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
    peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
    peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
    lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
    highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
    lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{

    for (auto* comp : getComps())
        addAndMakeVisible(comp);

    setSize (600, 400);
}

OloEQAudioProcessorEditor::~OloEQAudioProcessorEditor() {}

//==============================================================================
void OloEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void OloEQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() / 3);

    responseCurveComponent.setBounds(responseArea);

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() / 3);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() / 3);

    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() / 2));
    lowCutSlopeSlider.setBounds(lowCutArea);

    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() / 2));
    highCutSlopeSlider.setBounds(highCutArea);

    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() / 3));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() / 2));
    peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> OloEQAudioProcessorEditor::getComps()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider,
        &responseCurveComponent
    };
}
