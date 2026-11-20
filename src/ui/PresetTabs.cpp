#include "PresetTabs.h"
#include "../PluginProcessor.h"

namespace rainy
{
PresetTabs::PresetTabs (TapeLofiProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refreshMatch();
}

void PresetTabs::visibilityChanged()
{
    if (isShowing())
    {
        refreshMatch();
        startTimerHz (8);
    }
    else
        stopTimer();
}

void PresetTabs::timerCallback() { refreshMatch(); }

void PresetTabs::refreshMatch()
{
    const int m = processor.findMatchingPreset();
    if (m != matched)
    {
        matched = m;
        repaint();
    }
}

// ---- geometry -------------------------------------------------------------------------------
juce::Rectangle<float> PresetTabs::tabBounds (int index) const
{
    const int rows = (TapeLofiProcessor::getNumPresets() + kColumns - 1) / kColumns;
    const auto grid = getLocalBounds().withTrimmedLeft (kCaptionWidth).toFloat();
    const float w = (grid.getWidth() - (float) (kGap * (kColumns - 1))) / (float) kColumns;
    const float h = (grid.getHeight() - (float) (kGap * (rows - 1))) / (float) rows;
    const int col = index % kColumns, row = index / kColumns;
    return { grid.getX() + (float) col * (w + (float) kGap), grid.getY() + (float) row * (h + (float) kGap), w, h };
}

int PresetTabs::tabAt (juce::Point<int> pos) const
{
    for (int i = 0; i < TapeLofiProcessor::getNumPresets(); ++i)
        if (tabBounds (i).contains (pos.toFloat()))
            return i;
    return -1; // gaps and the caption column hit nothing
}

// ---- mouse ----------------------------------------------------------------------------------
void PresetTabs::mouseMove (const juce::MouseEvent& e)
{
    const int t = tabAt (e.getPosition());
    if (t != hovered)
    {
        hovered = t;
        repaint();
    }
}

void PresetTabs::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    pressed = -1;
    repaint();
}

void PresetTabs::mouseDown (const juce::MouseEvent& e)
{
    pressed = e.mods.isLeftButtonDown() ? tabAt (e.getPosition()) : -1;
    repaint();
}

void PresetTabs::mouseUp (const juce::MouseEvent& e)
{
    const int t = tabAt (e.getPosition());
    const bool click = pressed >= 0 && t == pressed;
    pressed = -1;

    if (click)
    {
        processor.loadPreset (t);   // UI-only path; the host-facing program API is untouched
        refreshMatch();
    }
    repaint();
}

juce::String PresetTabs::getTooltip()
{
    if (hovered < 0 || hovered >= TapeLofiProcessor::getNumPresets())
        return {};
    return juce::String::fromUTF8 (TapeLofiProcessor::getPreset (hovered).tipJp)
         + juce::String::fromUTF8 ("\n（クリックでつまみの値が切りかわります）");
}

// ---- drawing --------------------------------------------------------------------------------
void PresetTabs::drawTab (juce::Graphics& g, int i) const
{
    const auto& preset = TapeLofiProcessor::getPreset (i);
    const bool on = (i == matched), hot = (i == hovered), down = (i == pressed);

    auto r = tabBounds (i);
    if (down) r = r.translated (0.0f, 1.0f);

    // drop shadow
    g.setColour (juce::Colour (0x90000000));
    g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 2.0f), 5.0f);

    // paper label
    const auto top = on ? juce::Colour (0xfffbefc9) : hot ? juce::Colour (0xfff0e2bd) : juce::Colour (0xffd2bf93);
    const auto bot = on ? juce::Colour (0xffe9cf92) : hot ? juce::Colour (0xffcdb888) : juce::Colour (0xffa38c63);
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff2b1a11));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // ribbon stripe along the top edge (amber when this is the active sound)
    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle (r.reduced (0.5f), 4.5f);
    g.reduceClipRegion (clip);
    g.setColour (on ? col::amber : col::ribbon.withAlpha (0.85f));
    g.fillRect (r.withHeight (4.0f));
    g.restoreState();

    // lit lamp dot for the active tab
    if (on)
    {
        const auto c = juce::Point<float> (r.getRight() - 9.0f, r.getY() + 12.0f);
        g.setColour (juce::Colour (0x55ffa21f));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (c));
        g.setColour (juce::Colour (0xffffa21f));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));
    }

    // names
    const auto textArea = r.reduced (5.0f, 0.0f).withTrimmedTop (6.0f);
    g.setColour (col::ink);
    g.setFont (fonts->body (15.5f, 0.04f));
    g.drawText (juce::String::fromUTF8 (preset.nameJp), textArea.withHeight (textArea.getHeight() * 0.56f),
                juce::Justification::centred, true);
    g.setColour (col::ink.withAlpha (0.72f));
    g.setFont (fonts->body (9.5f, 0.04f));
    g.drawText (preset.nameEn, textArea.withTrimmedTop (textArea.getHeight() * 0.54f),
                juce::Justification::centredTop, true);
}

void PresetTabs::paint (juce::Graphics& g)
{
    // status caption (the static "PRESET" heading is engraved by PanelBackground)
    const auto cap = getLocalBounds().withWidth (kCaptionWidth - 12).withTrimmedTop (46).withHeight (36).toFloat();
    g.setFont (fonts->body (10.5f, 0.12f));
    g.setColour (col::creamDim);
    g.drawText ("NOW PLAYING", cap.withHeight (14.0f), juce::Justification::centredLeft, false);

    g.setFont (fonts->body (13.0f, 0.04f));
    g.setColour (matched >= 0 ? col::amberText : col::creamDim);
    g.drawText (matched >= 0 ? juce::String (TapeLofiProcessor::getPreset (matched).nameEn) : juce::String ("Custom"),
                cap.withTrimmedTop (15.0f), juce::Justification::centredLeft, true);

    for (int i = 0; i < TapeLofiProcessor::getNumPresets(); ++i)
        drawTab (g, i);
}
} // namespace rainy
