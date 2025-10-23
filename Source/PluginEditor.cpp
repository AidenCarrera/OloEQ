#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Colors
//==============================================================================

// General UI Colors
static const juce::Colour mainAccentColour        = juce::Colour(0xFF00FFCC); // #00FFCC
static const juce::Colour bodyBackgroundColour    = juce::Colour(0xFF1C1C1E); // #1C1C1E
static const juce::Colour headerBackgroundColour  = juce::Colour(0xFF18181a); // #18181A

// Rotary Dial Colors
static const juce::Colour dialFillColour          = juce::Colour(0xFF2A2A2C); // #2A2A2C - base fill for dials
static const juce::Colour dialOutlineColour       = juce::Colour(0xFF3A3A3E); // #3A3A3E - subtle border around dials
static const juce::Colour dialTickColour          = juce::Colour(0xFF00FFCC); // same as accent - tick/indicator
static const juce::Colour dialHighlightColour     = juce::Colour(0xFF00DDB3); // #00DDB3 - for active or hover ring
static const juce::Colour dialLabelTextColour     = juce::Colour(0xFFECECEC); // #ECECEC - label text under dials

//==============================================================================
// Custom LookAndFeel for all rotary sliders
//==============================================================================
class DialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g,
                      int x, int y, int width, int height,
                      float sliderPosProportional,
                      float rotaryStartAngle,
                      float rotaryEndAngle,
                      juce::Slider& slider) override
    {
        juce::ignoreUnused(slider);
        // Make a square bounds from the rectangle to keep circle
        float diameter = static_cast<float>(std::min(width, height)) - 8.0f; // 4px padding each side
        juce::Point<float> center(x + width * 0.5f, y + height * 0.5f);
        juce::Rectangle<float> knobBounds(center.x - diameter*0.5f, center.y - diameter*0.5f, diameter, diameter);

        // Draw base circle
        g.setColour(dialFillColour);
        g.fillEllipse(knobBounds);

        // Outline
        g.setColour(dialOutlineColour);
        g.drawEllipse(knobBounds, 1.5f);

        // Draw the knob indicator (tick/line)
        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        float radius = diameter / 2.0f - 2.0f; // keep indicator inside
        juce::Point<float> knobTip(center.x + std::cos(angle - juce::MathConstants<float>::halfPi) * radius,
                                center.y + std::sin(angle - juce::MathConstants<float>::halfPi) * radius);

        g.setColour(dialTickColour);
        g.drawLine(center.x, center.y, knobTip.x, knobTip.y, 2.0f);
    }

};


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

    g.fillAll(bodyBackgroundColour);

    auto responseArea = getLocalBounds();
    auto w = responseArea.getWidth();

    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    auto sampleRate = audioProcessor.getSampleRate();

    if (w <= 0)
        return;

    std::vector<float> mags(static_cast<size_t>(w), 1.0f);

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

        mags[static_cast<size_t>(i)] = juce::Decibels::gainToDecibels(mag);
    }

    juce::Path responseCurve;

    const float outputMin = static_cast<float>(responseArea.getBottom());
    const float outputMax = static_cast<float>(responseArea.getY());

    auto map = [outputMin, outputMax](float input)
    {
        return juce::jmap(input, -24.0f, 24.0f, outputMin, outputMax);
    };

    responseCurve.startNewSubPath(static_cast<float>(responseArea.getX()), map(mags.front()));

    const float startX = static_cast<float>(responseArea.getX());
    for (size_t i = 1; i < mags.size(); ++i)
        responseCurve.lineTo(startX + static_cast<float>(i), map(mags[i]));

    g.setColour(bodyBackgroundColour.brighter(0.08f));
    for (int y = 0; y < 5; ++y)
    {
        float pos = juce::jmap(static_cast<float>(y), 0.0f, 4.0f, outputMax, outputMin);
        g.drawHorizontalLine(static_cast<int>(pos), static_cast<float>(responseArea.getX()), static_cast<float>(responseArea.getRight()));
    }

    g.setColour(mainAccentColour);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);

    g.setColour(mainAccentColour.contrasting(0.6f));
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

    // Apply custom LookAndFeel to all rotary sliders
    static auto myDialLookAndFeel = std::make_unique<DialLookAndFeel>();
    peakFreqSlider.setLookAndFeel(myDialLookAndFeel.get());
    peakGainSlider.setLookAndFeel(myDialLookAndFeel.get());
    peakQualitySlider.setLookAndFeel(myDialLookAndFeel.get());
    lowCutFreqSlider.setLookAndFeel(myDialLookAndFeel.get());
    highCutFreqSlider.setLookAndFeel(myDialLookAndFeel.get());
    lowCutSlopeSlider.setLookAndFeel(myDialLookAndFeel.get());
    highCutSlopeSlider.setLookAndFeel(myDialLookAndFeel.get());

    setSize (600, 400);
}

OloEQAudioProcessorEditor::~OloEQAudioProcessorEditor()
{
    // Reset look and feel to avoid dangling pointers
    peakFreqSlider.setLookAndFeel(nullptr);
    peakGainSlider.setLookAndFeel(nullptr);
    peakQualitySlider.setLookAndFeel(nullptr);
    lowCutFreqSlider.setLookAndFeel(nullptr);
    highCutFreqSlider.setLookAndFeel(nullptr);
    lowCutSlopeSlider.setLookAndFeel(nullptr);
    highCutSlopeSlider.setLookAndFeel(nullptr);
}

//==============================================================================

void OloEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(bodyBackgroundColour);

    auto headerArea = getLocalBounds().removeFromTop(48);
    g.setColour(headerBackgroundColour);
    g.fillRect(headerArea);

    g.setColour(mainAccentColour.darker(0.1f));
    juce::Font titleFont(juce::FontOptions().withHeight(20.0f).withMetricsKind(juce::TypefaceMetricsKind::legacy));
    titleFont.setBold(true);
    g.setFont(titleFont);
    g.setFont(titleFont);
    g.drawText("OloEQ", headerArea, juce::Justification::centred, false);
}

void OloEQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    auto headerArea = bounds.removeFromTop(48);
    responseCurveComponent.setBounds(bounds.removeFromTop(bounds.getHeight() / 3));

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
