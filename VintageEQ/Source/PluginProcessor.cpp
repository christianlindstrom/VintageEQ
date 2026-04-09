/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VintageEQAudioProcessor::VintageEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

// setup of bands
juce::AudioProcessorValueTreeState::ParameterLayout VintageEQAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // High-pass frequency - three choices
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "hp_freq", "HP Freq",
        juce::StringArray{ "20 Hz", "80 Hz", "120 Hz" }, 0));

    // 200Hz cut
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "low_mid_gain", "200Hz Gain",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f), 0.0f));

    // 600Hz band
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mid_gain", "600Hz Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    // 2kHz presence
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "presence_gain", "2kHz Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    // High shelf 10kHz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "high_shelf_gain", "10kHz Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    // Saturation drive
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "drive", "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

VintageEQAudioProcessor::~VintageEQAudioProcessor()
{
}

//==============================================================================
const juce::String VintageEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VintageEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VintageEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VintageEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VintageEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VintageEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int VintageEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VintageEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String VintageEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void VintageEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void VintageEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Reset filter states
    for (int band = 0; band < 5; ++band)
        for (int ch = 0; ch < 2; ++ch)
            x1[band][ch] = x2[band][ch] = y1[band][ch] = y2[band][ch] = 0.0f;

    updateCoefficients(sampleRate);
}

void VintageEQAudioProcessor::calcPeak(int band, double f0, double dBgain, double Q, double sampleRate)
{
    double A = std::pow(10.0, dBgain / 40.0);
    double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
    double alpha = std::sin(w0) / (2.0 * Q);
    double a0d = 1.0 + alpha / A;

    b0[band] = (float)((1.0 + alpha * A) / a0d);
    b1[band] = (float)((-2.0 * std::cos(w0)) / a0d);
    b2[band] = (float)((1.0 - alpha * A) / a0d);
    a1[band] = (float)((-2.0 * std::cos(w0)) / a0d);
    a2[band] = (float)((1.0 - alpha / A) / a0d);
}

void VintageEQAudioProcessor::calcHighShelf(int band, double f0, double dBgain, double sampleRate)
{
    double A = std::pow(10.0, dBgain / 40.0);
    double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
    double alpha = std::sin(w0) / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / 0.707 - 1.0) + 2.0);
    double a0d = (A + 1) - (A - 1) * std::cos(w0) + 2 * std::sqrt(A) * alpha;

    b0[band] = (float)(A * ((A + 1) + (A - 1) * std::cos(w0) + 2 * std::sqrt(A) * alpha) / a0d);
    b1[band] = (float)(-2 * A * ((A - 1) + (A + 1) * std::cos(w0)) / a0d);
    b2[band] = (float)(A * ((A + 1) + (A - 1) * std::cos(w0) - 2 * std::sqrt(A) * alpha) / a0d);
    a1[band] = (float)(2 * ((A - 1) - (A + 1) * std::cos(w0)) / a0d);
    a2[band] = (float)((A + 1) - (A - 1) * std::cos(w0) - 2 * std::sqrt(A) * alpha) / a0d;
}

void VintageEQAudioProcessor::calcHP(int band, double f0, double sampleRate)
{
    double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / sampleRate;
    double alpha = std::sin(w0) / (2.0 * 0.707);
    double a0d = 1.0 + alpha;

    b0[band] = (float)((1.0 + std::cos(w0)) / 2.0 / a0d);
    b1[band] = (float)(-(1.0 + std::cos(w0)) / a0d);
    b2[band] = (float)((1.0 + std::cos(w0)) / 2.0 / a0d);
    a1[band] = (float)((-2.0 * std::cos(w0)) / a0d);
    a2[band] = (float)((1.0 - alpha) / a0d);
}

void VintageEQAudioProcessor::updateCoefficients(double sampleRate)
{
    auto hpChoice = (int)apvts.getRawParameterValue("hp_freq")->load();
    auto lowMidGain = (double)apvts.getRawParameterValue("low_mid_gain")->load();
    auto midGain = (double)apvts.getRawParameterValue("mid_gain")->load();
    auto presenceGain = (double)apvts.getRawParameterValue("presence_gain")->load();
    auto shelfGain = (double)apvts.getRawParameterValue("high_shelf_gain")->load();

    double hpFreq = 20.0;
    if (hpChoice == 1) hpFreq = 80.0;
    else if (hpChoice == 2) hpFreq = 120.0;

    calcHP(0, hpFreq, sampleRate);
    calcPeak(1, 200.0, lowMidGain, 1.5, sampleRate);
    calcPeak(2, 650.0, midGain, 0.7, sampleRate);
    calcPeak(3, 2000.0, presenceGain, 0.9, sampleRate);
    calcHighShelf(4, 10000.0, shelfGain, sampleRate);
}


void VintageEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool VintageEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void VintageEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    updateCoefficients(getSampleRate());
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float sig = channelData[sample];

            for (int band = 0; band < 5; ++band)
            {
                float out = b0[band] * sig
                    + b1[band] * x1[band][channel]
                    + b2[band] * x2[band][channel]
                    - a1[band] * y1[band][channel]
                    - a2[band] * y2[band][channel];

                x2[band][channel] = x1[band][channel];
                x1[band][channel] = sig;
                y2[band][channel] = y1[band][channel];
                y1[band][channel] = out;

                sig = out;
            }

            // Saturation
            float drive = apvts.getRawParameterValue("drive")->load();
            float driveAmount = 1.0f + drive * 0.008f;
            sig = std::tanh(sig * driveAmount) / driveAmount;

            channelData[sample] = sig;
        }
    }
}

//==============================================================================
bool VintageEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* VintageEQAudioProcessor::createEditor()
{
    return new VintageEQAudioProcessorEditor (*this);
}

//==============================================================================
void VintageEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void VintageEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VintageEQAudioProcessor();
}
