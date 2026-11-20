#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RainyTheme.h"
#include "RainyLookAndFeel.h"

namespace rainy
{
// How a knob's value is written on its plate. At most one decimal place.
enum class ValueKind { percent, signedPercent, decibel, noiseDb, hertz, plain };

constexpr int kNameStripHeight = 22;

// One rotary knob + value plate, bound to a parameter.
class KnobCell final : public juce::Component, public juce::SettableTooltipClient
{
public:
    KnobCell (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
              ValueKind, const juce::String& tooltip);
    void resized() override;

private:
    static juce::String format (ValueKind, double);
    static double parse (ValueKind, const juce::String&, double minimum);

    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Round amber lamp that doubles as the power switch.
class PowerLamp final : public juce::ToggleButton
{
public:
    PowerLamp();
    void paint (juce::Graphics&) override;
};

// Two-position SLOW / FAST switch.
class SpeedSwitch final : public juce::ToggleButton
{
public:
    SpeedSwitch();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::SharedResourcePointer<FontCache> fonts;
};

// Little cassette window with turning reels (decorative, follows Power / Tape Speed).
class ReelWindow final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit ReelWindow (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void drawReel (juce::Graphics&, juce::Point<float> centre, float tapeRadius, float angle) const;

    std::atomic<float>* power = nullptr;
    std::atomic<float>* speed = nullptr;
    float angle = 0.0f;
    double lastTime = 0.0;
    juce::SharedResourcePointer<FontCache> fonts;
};

// Static artwork: textured panel, engraved titles, dividers, screws, wear overlay.
class PanelBackground final : public juce::Component
{
public:
    struct Label
    {
        juce::String text;
        juce::Rectangle<float> area;
        float size = 13.0f;
        float tracking = 0.0f;
        juce::Justification just = juce::Justification::centred;
        juce::Colour colour = col::cream;
    };

    PanelBackground (juce::Image paint, juce::Image wear);

    void addLabel (Label l) { labels.push_back (std::move (l)); }
    void addHeading (const juce::String& en, const juce::String& jp, juce::Rectangle<float> area);
    void addVerticalGroove (float x, float y0, float y1) { vGrooves.push_back ({ x, y0, y1 }); }
    void addHorizontalGroove (float y, float x0, float x1) { hGrooves.push_back ({ y, x0, x1 }); }

    void paint (juce::Graphics&) override;

private:
    struct Groove { float pos, from, to; };
    struct Heading { juce::String en, jp; juce::Rectangle<float> area; };

    void drawEngravedText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&,
                           juce::Justification, juce::Colour) const;
    void drawTitle (juce::Graphics&) const;
    static void drawSparkle (juce::Graphics&, juce::Point<float> c, float r);
    static void drawScrew (juce::Graphics&, juce::Point<float> c, float angle);
    static void drawGroove (juce::Graphics&, juce::Line<float>, bool horizontal);

    juce::Image paintImage, wearImage;
    std::vector<Label> labels;
    std::vector<Groove> vGrooves, hGrooves;
    std::vector<Heading> headings;
    juce::SharedResourcePointer<FontCache> fonts;
};
} // namespace rainy
