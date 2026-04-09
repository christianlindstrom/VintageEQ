/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class VintageEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    VintageEQAudioProcessor();
    ~VintageEQAudioProcessor() override;
    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    struct BandCoeffs
    {
        float b0, b1, b2, a1, a2;
    };

    std::array<BandCoeffs, 5> getBandCoeffs() const
    {
        std::array<BandCoeffs, 5> coeffs;
        for (int i = 0; i < 5; ++i)
            coeffs[i] = { b0[i], b1[i], b2[i], a1[i], a2[i] };
        return coeffs;
    }

private:
    //==============================================================================
   // Filter state per channel (0=left, 1=right), per band
    // Bands: 0=HP, 1=200Hz, 2=600Hz, 3=2kHz, 4=10kHz shelf
    float x1[5][2] = {}, x2[5][2] = {};
    float y1[5][2] = {}, y2[5][2] = {};

    // Coefficients per band
    float b0[5] = { 1,1,1,1,1 };
    float b1[5] = {}, b2[5] = {};
    float a1[5] = {}, a2[5] = {};

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateCoefficients(double sampleRate);
    void calcPeak(int band, double f0, double dBgain, double Q, double sampleRate);
    void calcHighShelf(int band, double f0, double dBgain, double sampleRate);
    void calcHP(int band, double f0, double sampleRate);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageEQAudioProcessor)
};
