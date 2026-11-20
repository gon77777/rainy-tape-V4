#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "RainyTheme.h"

class TapeLofiProcessor;

namespace rainy
{
// The preset shelf: a status caption on the left and a 6 x 2 grid of cassette-label tabs.
// It lives in its own strip below the knob panel, so it never overlaps any other control.
//
// The highlighted tab is not stored anywhere: it is recomputed from the live parameter values
// (TapeLofiProcessor::findMatchingPreset), so it follows host automation, undo and state restore
// without any of those paths having to know about presets. Moving a knob un-highlights the tab.
class PresetTabs final : public juce::Component, public juce::TooltipClient, private juce::Timer
{
public:
    explicit PresetTabs (TapeLofiProcessor&);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    juce::String getTooltip() override;

    static constexpr int kColumns = 6;
    static constexpr int kCaptionWidth = 116;   // left column (status text)
    static constexpr int kGap = 6;

private:
    void timerCallback() override;
    void refreshMatch();
    int tabAt (juce::Point<int>) const;
    juce::Rectangle<float> tabBounds (int index) const;
    void drawTab (juce::Graphics&, int index) const;

    TapeLofiProcessor& processor;
    juce::SharedResourcePointer<FontCache> fonts;
    int matched = -1, hovered = -1, pressed = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetTabs)
};
} // namespace rainy
