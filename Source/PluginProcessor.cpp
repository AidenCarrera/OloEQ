/*
  ==============================================================================

    PluginProcessor.cpp - Cleaned and aligned with PluginEditor style

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "JucePluginDefines.h"

//==============================================================================
// Constructor / Destructor
OloEQAudioProcessor::OloEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
}

OloEQAudioProcessor::~OloEQAudioProcessor() {}

//==============================================================================
// Plugin information
const juce::String OloEQAudioProcessor::getName() const { return JucePlugin_Name; }
bool OloEQAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}
bool OloEQAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}
bool OloEQAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}
double OloEQAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int OloEQAudioProcessor::getNumPrograms() { return 1; }
int OloEQAudioProcessor::getCurrentProgram() { return 0; }
void OloEQAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String OloEQAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void OloEQAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
// Prepare / release resources
void OloEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters();
}

void OloEQAudioProcessor::releaseResources() {}

//==============================================================================
// Check bus layouts
#ifndef JucePlugin_PreferredChannelConfigurations
bool OloEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    auto& mainOut = layouts.getMainOutputChannelSet();
    auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (mainOut != mainIn)
        return false;
#endif

    return true;
#endif
}
#endif

//==============================================================================
// Main audio processing
void OloEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalInputChannels  = getTotalNumInputChannels();
    auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalInputChannels; i < totalOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock  = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);

    juce::ignoreUnused(midiMessages);
}

//==============================================================================
// Editor
bool OloEQAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* OloEQAudioProcessor::createEditor() { return new OloEQAudioProcessorEditor(*this); }

//==============================================================================
// State management
void OloEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void OloEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        updateFilters();
    }
}

//==============================================================================
// Parameter helpers
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    return settings;
}

Coefficients makePeakFilter(const ChainSettings& settings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, settings.peakFreq, settings.peakQuality,
        juce::Decibels::decibelsToGain(settings.peakGainInDecibels)
    );
}

//==============================================================================
// Filter updates
void OloEQAudioProcessor::updatePeakFilter(const ChainSettings& settings)
{
    auto peakCoefficients = makePeakFilter(settings, getSampleRate());

    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void updateCoefficients(Coefficients& old, const Coefficients& replacements)
{
    *old = *replacements;
}

void OloEQAudioProcessor::updateLowCutFilters(const ChainSettings& settings)
{
    auto lowCutCoefficients = makeLowCutFilter(settings, getSampleRate());

    updateCutFilter(leftChain.get<ChainPositions::LowCut>(), lowCutCoefficients, settings.lowCutSlope);
    updateCutFilter(rightChain.get<ChainPositions::LowCut>(), lowCutCoefficients, settings.lowCutSlope);
}

void OloEQAudioProcessor::updateHighCutFilters(const ChainSettings& settings)
{
    auto highCutCoefficients = makeHighCutFilter(settings, getSampleRate());

    updateCutFilter(leftChain.get<ChainPositions::HighCut>(), highCutCoefficients, settings.highCutSlope);
    updateCutFilter(rightChain.get<ChainPositions::HighCut>(), highCutCoefficients, settings.highCutSlope);
}

void OloEQAudioProcessor::updateFilters()
{
    auto settings = getChainSettings(apvts);

    updateLowCutFilters(settings);
    updatePeakFilter(settings);
    updateHighCutFilters(settings);
}

//==============================================================================
// Create parameter layout
juce::AudioProcessorValueTreeState::ParameterLayout
OloEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Freq", "Peak Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 750.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Gain", "Peak Gain", juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Quality", "Peak Quality", juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));

    juce::StringArray slopeChoices;
    for (int i = 0; i < 4; ++i)
        slopeChoices.add(juce::String(12 + i * 12) + " db/Oct");

    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", slopeChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", slopeChoices, 0));

    return layout;
}

//==============================================================================
// Factory function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OloEQAudioProcessor(); }
