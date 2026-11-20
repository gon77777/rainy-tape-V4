#pragma once

#include "RainyTheme.h"

namespace rainy
{
// Knobs, value plates and tooltips in a worn, brown, analogue-hardware style.
class RainyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RainyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    juce::Font getLabelFont (juce::Label&) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    static void drawLamp (juce::Graphics&, juce::Rectangle<float> area, bool on);

private:
    juce::TextLayout tooltipLayout (const juce::String& text) const;

    juce::SharedResourcePointer<FontCache> fonts;
};
} // namespace rainy
