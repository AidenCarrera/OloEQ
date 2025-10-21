/*
  ==============================================================================

    PluginProcessor.h
    Defines the main audio processor for the OloEQ plugin, including
    filter chains, parameter handling, and DSP helper functions.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
// Filter slope options
enum Slope
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

//==============================================================================
// All chain settings
struct ChainSettings
{
    float peakFreq{ 0 }, peakGainInDecibels{ 0 }, peakQuality{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    Slope lowCutSlope{ Slope_12 }, highCutSlope{ Slope_12 };
};

//==============================================================================
// Retrieve current chain settings from the APVTS
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
// Type aliases for DSP
using Filter     = juce::dsp::IIR::Filter<float>;
using CutFilter  = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
using MonoChain  = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
using Coefficients = Filter::CoefficientsPtr;

//==============================================================================
// Chain positions
enum ChainPositions
{
    LowCut,
    Peak,
    HighCut
};

//==============================================================================
// Filter helpers
void updateCoefficients(Coefficients& old, const Coefficients& replacements);
Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate);

//==============================================================================
// Update helpers for cut filters
template<int Index, typename ChainType, typename CoefficientType>
void update(ChainType& chain, const CoefficientType& coefficients)
{
    updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
    chain.template setBypassed<Index>(false);
}

template<typename ChainType, typename CoefficientType>
void updateCutFilter(ChainType& chain, const CoefficientType& coefficients, const Slope& slope)
{
    // Bypass all filters initially
    chain.template setBypassed<0>(true);
    chain.template setBypassed<1>(true);
    chain.template setBypassed<2>(true);
    chain.template setBypassed<3>(true);

    // Enable filters according to slope
    switch (slope)
    {
        case Slope_48: update<3>(chain, coefficients);
        case Slope_36: update<2>(chain, coefficients);
        case Slope_24: update<1>(chain, coefficients);
        case Slope_12: update<0>(chain, coefficients);
    }
}

//==============================================================================
// Convenience factory methods for Butterworth filters
inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        chainSettings.lowCutFreq, sampleRate, 2 * chainSettings.lowCutSlope + 1
    );
}

inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
        chainSettings.highCutFreq, sampleRate, 2 * chainSettings.highCutSlope + 1
    );
}

//==============================================================================
// Main processor class
class OloEQAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    OloEQAudioProcessor();
    ~OloEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:
    //==============================================================================
    MonoChain leftChain, rightChain;

    void updatePeakFilter(const ChainSettings& chainSettings);
    void updateLowCutFilters(const ChainSettings& chainSettings);
    void updateHighCutFilters(const ChainSettings& chainSettings);
    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OloEQAudioProcessor)
};
