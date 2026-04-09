#include "PluginProcessor.h"
#include "PluginEditor.h"

// Vintage colour palette
static const juce::Colour panelColour{ 0xff2d1f0e };  // dark tobacco brown
static const juce::Colour knobColour{ 0xfff0e6c8 };  // warm cream
static const juce::Colour textColour{ 0xfff0e6c8 };  // same cream
static const juce::Colour accentColour{ 0xffcc8833 };  // amber
static const juce::Colour buttonOff{ 0xff4a3520 };  // dark brown
static const juce::Colour buttonOn{ 0xffcc8833 };  // amber

//==============================================================================
// Custom LookAndFeel for vintage knobs
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider&) override
    {
        auto radius = (float)juce::jmin(width / 2, height / 2) - 6.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Yttre skugga
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillEllipse(rx + 4, ry + 4, rw, rw);

        // Yttre ring - mörk brons
        juce::ColourGradient ringGrad(
            juce::Colour(0xff8a6a2a), centreX - radius, centreY - radius,
            juce::Colour(0xff3a2a0a), centreX + radius, centreY + radius, false);
        g.setGradientFill(ringGrad);
        g.fillEllipse(rx, ry, rw, rw);

        // Knob-kropp - creme gradient
        float innerR = radius * 0.88f;
        juce::ColourGradient bodyGrad(
            juce::Colour(0xfffff8e8), centreX - innerR * 0.5f, centreY - innerR * 0.5f,
            juce::Colour(0xffd4c89a), centreX + innerR * 0.5f, centreY + innerR * 0.5f, false);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(centreX - innerR, centreY - innerR, innerR * 2, innerR * 2);

        // Highlight - liten vit reflex uppe till vänster
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillEllipse(centreX - innerR * 0.55f, centreY - innerR * 0.65f,
            innerR * 0.5f, innerR * 0.35f);

        // Subtil rim-skugga på knob-kroppen
        g.setColour(juce::Colours::black.withAlpha(0.2f));
        g.drawEllipse(centreX - innerR, centreY - innerR, innerR * 2, innerR * 2, 1.0f);

        // Pekare
        juce::Path p;
        auto pointerLength = innerR * 0.65f;
        auto pointerThickness = 2.5f;
        p.addRectangle(-pointerThickness * 0.5f, -innerR + 2.0f, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colour(0xff2a1a05));
        g.fillPath(p);

        // Centerpunkt - amber
        g.setColour(juce::Colour(0xffcc8833));
        g.fillEllipse(centreX - 2.5f, centreY - 2.5f, 5.0f, 5.0f);

        // Markering runt yttre ring - liten prick vid pekaren
        juce::Path dot;
        dot.addEllipse(-2.0f, -radius + 1.0f, 4.0f, 4.0f);
        dot.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colour(0xffcc8833));
        g.fillPath(dot);
    }
};

//==============================================================================
VintageEQAudioProcessorEditor::VintageEQAudioProcessorEditor(VintageEQAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), frequencyDisplay (p)
{
    vintageLookAndFeel = std::make_unique<VintageLookAndFeel>();
    setLookAndFeel(vintageLookAndFeel.get());
    addAndMakeVisible(frequencyDisplay);

    // Setup knob helper lambda
    auto setupKnob = [&](juce::Slider& knob, juce::Label& label, const juce::String& text)
        {
            knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
            knob.setColour(juce::Slider::textBoxTextColourId, textColour);
            knob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible(knob);

            label.setText(text, juce::dontSendNotification);
            label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            label.setColour(juce::Label::textColourId, textColour);
            label.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(label);
        };

    setupKnob(lowMidKnob, lowMidLabel, "200 Hz");
    setupKnob(midKnob, midLabel, "650 Hz");
    setupKnob(presenceKnob, presenceLabel, "2 kHz");
    setupKnob(shelfKnob, shelfLabel, "10 kHz");
    setupKnob(driveKnob, driveLabel, "Drive");

    // HP buttons
    auto setupHPButton = [&](juce::TextButton& btn)
        {
            btn.setColour(juce::TextButton::buttonColourId, buttonOff);
            btn.setColour(juce::TextButton::buttonOnColourId, buttonOn);
            btn.setColour(juce::TextButton::textColourOffId, textColour);
            btn.setColour(juce::TextButton::textColourOnId, panelColour);
            btn.setClickingTogglesState(true);
            btn.setRadioGroupId(1);
            addAndMakeVisible(btn);
        };

    setupHPButton(hp20Button);
    setupHPButton(hp80Button);
    setupHPButton(hp120Button);
    hp20Button.setToggleState(true, juce::dontSendNotification);

    // Attachments
    auto& apvts = audioProcessor.apvts;
    lowMidAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "low_mid_gain", lowMidKnob);
    midAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mid_gain", midKnob);
    presenceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "presence_gain", presenceKnob);
    shelfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "high_shelf_gain", shelfKnob);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "drive", driveKnob);
    hp20Button.onClick = [&]() { audioProcessor.apvts.getParameterAsValue("hp_freq") = 0; };
    hp80Button.onClick = [&]() { audioProcessor.apvts.getParameterAsValue("hp_freq") = 1; };
    hp120Button.onClick = [&]() { audioProcessor.apvts.getParameterAsValue("hp_freq") = 2; };

    setSize(680, 350);
}

VintageEQAudioProcessorEditor::~VintageEQAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void VintageEQAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Huvudgradient - mörk tobak uppifrån, lite varmare nedtill
    juce::ColourGradient panelGrad(
        juce::Colour(0xff1a0a02), 0, 0,
        juce::Colour(0xff2d1a08), 0, (float)getHeight(), false);
    g.setGradientFill(panelGrad);
    g.fillAll();

    // Subtil horisontell brus-textur - tunna linjer
    g.setColour(juce::Colours::white.withAlpha(0.015f));
    for (int y = 0; y < getHeight(); y += 2)
        g.drawHorizontalLine(y, 0, (float)getWidth());

    // Yttre ram - brons
    juce::ColourGradient rimGrad(
        juce::Colour(0xffaa7722), 0, 0,
        juce::Colour(0xff553311), 0, (float)getHeight(), false);
    g.setGradientFill(rimGrad);
    g.drawRoundedRectangle(bounds.reduced(1), 4.0f, 2.0f);

    // Inre panel - nedsänkt känsla
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(4), 3.0f, 1.0f);

    // Plugin-namn
    g.setColour(juce::Colour(0xffcc8833));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawFittedText("VINTAGE EQ", 12, 8, 120, 18, juce::Justification::left, 1);

    // Subtil understrykning av titeln
    g.setColour(juce::Colour(0xffcc8833).withAlpha(0.4f));
    g.drawHorizontalLine(27, 12, 95);

    // HP-sektion label
    g.setColour(juce::Colour(0xfff0e6c8).withAlpha(0.8f));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawFittedText("H I G H", 14, 188, 52, 12, juce::Justification::centred, 1);
    g.drawFittedText("P A S S", 14, 200, 52, 12, juce::Justification::centred, 1);
    g.drawFittedText("H z", 14, 292, 52, 12, juce::Justification::centred, 1);

    // Separator vänster - graverad känsla
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(82, 22, 82, (float)getHeight() - 10, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawLine(84, 22, 84, (float)getHeight() - 10, 1.0f);

    // Separator höger
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(548, 22, 548, (float)getHeight() - 10, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawLine(550, 22, 550, (float)getHeight() - 10, 1.0f);

    // Display-ram highlight
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(9, 29, getWidth() - 18, 122, 4.0f, 1.0f);

    // Version och copyright - nedre högra hörnet
    g.setColour(juce::Colour(0xffcc8833).withAlpha(0.5f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawFittedText("v1.0.0  (c) Christian L 2026",
        getWidth() - 160, getHeight() - 18,
        150, 14,
        juce::Justification::right, 1);
}
void VintageEQAudioProcessorEditor::resized()
{
    frequencyDisplay.setBounds(10, 30, getWidth() - 20, 120);

    // HP buttons
    hp20Button.setBounds(18, 220, 44, 22);
    hp80Button.setBounds(18, 246, 44, 22);
    hp120Button.setBounds(18, 272, 44, 22);

    // Knobs
    int knobY = 165;
    int knobSize = 100;
    int labelH = 18;
    int startX = 90;
    int spacing = 110;

    lowMidKnob.setBounds(startX + spacing * 0, knobY, knobSize, knobSize);
    midKnob.setBounds(startX + spacing * 1, knobY, knobSize, knobSize);
    presenceKnob.setBounds(startX + spacing * 2, knobY, knobSize, knobSize);
    shelfKnob.setBounds(startX + spacing * 3, knobY, knobSize, knobSize);

    lowMidLabel.setBounds(startX + spacing * 0, knobY + knobSize, knobSize, labelH);
    midLabel.setBounds(startX + spacing * 1, knobY + knobSize, knobSize, labelH);
    presenceLabel.setBounds(startX + spacing * 2, knobY + knobSize, knobSize, labelH);
    shelfLabel.setBounds(startX + spacing * 3, knobY + knobSize, knobSize, labelH);

    // Drive
    driveKnob.setBounds(548, knobY, knobSize, knobSize);
    driveLabel.setBounds(548, knobY + knobSize, knobSize, labelH);
}