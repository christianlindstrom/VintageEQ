#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Forward declarations
class VintageLookAndFeel;

class FrequencyDisplay : public juce::Component,
    public juce::Timer
{
public:
    FrequencyDisplay(VintageEQAudioProcessor& p) : processor(p)
    {
        startTimerHz(30); // uppdatera 30 gånger per sekund
    }

    void timerCallback() override
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Bakgrund
        g.setColour(juce::Colour(0xff1a0f05));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Ram
        g.setColour(juce::Colour(0xffcc8833).withAlpha(0.4f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // Hämta koefficienter
        auto coeffs = processor.getBandCoeffs();
        double sr = processor.getSampleRate();
        if (sr <= 0) sr = 44100.0;

        int w = getWidth();
        int h = getHeight();

        // Rita rutnät
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        {
            float y = juce::jmap(db, -18.0f, 18.0f, (float)h - 4, 4.0f);
            g.drawHorizontalLine((int)y, 4.0f, (float)w - 4);
        }

        // Rita 0dB-linje lite tydligare
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        float zeroY = juce::jmap(0.0f, -18.0f, 18.0f, (float)h - 4, 4.0f);
        g.drawHorizontalLine((int)zeroY, 4.0f, (float)w - 4);

        // Beräkna och rita kurvan
        juce::Path curve;
        bool started = false;

        for (int px = 0; px < w; ++px)
        {
            // Frekvens för denna pixel (logaritmisk skala 20Hz - 20kHz)
            double freq = 20.0 * std::pow(1000.0, (double)px / (double)w);
            double omega = 2.0 * juce::MathConstants<double>::pi * freq / sr;

            // Beräkna kombinerad respons för alla band
            double totalDb = 0.0;

            for (int band = 0; band < 5; ++band)
            {
                auto& c = coeffs[band];
                // H(z) för biquad vid frekvens omega
                double cosw = std::cos(omega);
                double cos2w = std::cos(2.0 * omega);
                double sinw = std::sin(omega);
                double sin2w = std::sin(2.0 * omega);

                double numR = c.b0 + c.b1 * cosw + c.b2 * cos2w;
                double numI = c.b1 * (-sinw) + c.b2 * (-sin2w);
                double denR = 1.0 + c.a1 * cosw + c.a2 * cos2w;
                double denI = c.a1 * (-sinw) + c.a2 * (-sin2w);

                double numMag = std::sqrt(numR * numR + numI * numI);
                double denMag = std::sqrt(denR * denR + denI * denI);

                if (denMag > 0.0001)
                    totalDb += 20.0 * std::log10(numMag / denMag);
            }

            float y = juce::jmap((float)totalDb, -18.0f, 18.0f, (float)h - 4, 4.0f);
            y = juce::jlimit(2.0f, (float)h - 2, y);

            if (!started)
            {
                curve.startNewSubPath((float)px, y);
                started = true;
            }
            else
            {
                curve.lineTo((float)px, y);
            }
        }

        // Fylld area under kurvan
        juce::Path filled = curve;
        filled.lineTo((float)w, zeroY);
        filled.lineTo(0.0f, zeroY);
        filled.closeSubPath();
        g.setColour(juce::Colour(0xffcc8833).withAlpha(0.15f));
        g.fillPath(filled);

        // Kurvan själv
        g.setColour(juce::Colour(0xffcc8833));
        g.strokePath(curve, juce::PathStrokeType(1.5f));
    }

private:
    VintageEQAudioProcessor& processor;
};

class VintageEQAudioProcessorEditor : public juce::AudioProcessorEditor

{
public:
    VintageEQAudioProcessorEditor(VintageEQAudioProcessor&);
    ~VintageEQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VintageEQAudioProcessor& audioProcessor;
    std::unique_ptr<VintageLookAndFeel> vintageLookAndFeel;
    FrequencyDisplay frequencyDisplay;

    // Rotary sliders for gain bands
    juce::Slider lowMidKnob, midKnob, presenceKnob, shelfKnob, driveKnob;

    // Labels
    juce::Label lowMidLabel, midLabel, presenceLabel, shelfLabel, driveLabel;

    // HP frequency buttons
    juce::TextButton hp20Button{ "20" };
    juce::TextButton hp80Button{ "80" };
    juce::TextButton hp120Button{ "120" };

    // Attachments - kopplar UI till APVTS
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> presenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shelfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageEQAudioProcessorEditor)
};