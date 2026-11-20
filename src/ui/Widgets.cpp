#include "Widgets.h"

namespace rainy
{
// =============================================================================================
// KnobCell
// =============================================================================================
juce::String KnobCell::format (ValueKind kind, double v)
{
    if (std::abs (v) < 0.05) v = 0.0; // avoid "-0.0"

    switch (kind)
    {
        case ValueKind::percent:       return juce::String (v, 1) + " %";
        case ValueKind::signedPercent: return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " %";
        case ValueKind::decibel:       return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " dB";
        case ValueKind::noiseDb:       return v <= -119.95 ? juce::String ("Off") : juce::String (v, 1) + " dB";
        case ValueKind::hertz:         return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz"
                                                          : juce::String (v, 1) + " Hz";
        case ValueKind::plain:         return juce::String (v, 1);
    }
    return {};
}

double KnobCell::parse (ValueKind kind, const juce::String& text, double minimum)
{
    if (kind == ValueKind::noiseDb && text.containsIgnoreCase ("off"))
        return minimum;

    double v = text.retainCharacters ("0123456789.-+").getDoubleValue();
    if (kind == ValueKind::hertz && text.containsIgnoreCase ("k"))
        v *= 1000.0;
    return v;
}

KnobCell::KnobCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                    ValueKind kind, const juce::String& tip)
{
    setTooltip (tip);
    slider.setTooltip (tip); // must be set before the text box is (re)built

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setScrollWheelEnabled (true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
    slider.setTextBoxIsEditable (true);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

    // The attachment installs 2-decimal formatting; replace it (one decimal at most).
    slider.textFromValueFunction = [kind] (double v) { return format (kind, v); };
    slider.valueFromTextFunction = [kind, this] (const juce::String& t) { return parse (kind, t, slider.getMinimum()); };
    slider.updateText();
}

void KnobCell::resized()
{
    // the top strip is left free for the printed knob name (drawn by PanelBackground)
    slider.setBounds (getLocalBounds().withTrimmedTop (kNameStripHeight));
}

// =============================================================================================
// PowerLamp / SpeedSwitch
// =============================================================================================
PowerLamp::PowerLamp()
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PowerLamp::paint (juce::Graphics& g)
{
    RainyLookAndFeel::drawLamp (g, getLocalBounds().toFloat(), getToggleState());
}

SpeedSwitch::SpeedSwitch()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SpeedSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isLeftButtonDown())
        return;

    const bool fast = e.x >= getWidth() / 2;
    if (fast != getToggleState())
        setToggleState (fast, juce::sendNotificationSync);
}

void SpeedSwitch::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool fast = getToggleState();

    // lip + recessed well
    g.setColour (juce::Colour (0x30f0dcb0));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 7.0f);
    g.setColour (col::plate);
    g.fillRoundedRectangle (r, 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x90000000), r.getX(), r.getY(),
                                             juce::Colour (0x00000000), r.getX(), r.getY() + 12.0f, false));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (juce::Colour (0xff0e0805));
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    // lever
    const auto half = r.reduced (3.0f).withWidth ((r.getWidth() - 6.0f) * 0.5f);
    const auto lever = fast ? half.translated (half.getWidth(), 0.0f) : half;
    g.setColour (juce::Colour (0x80000000));
    g.fillRoundedRectangle (lever.translated (0.0f, 1.5f), 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0e2bd), lever.getX(), lever.getY(),
                                             juce::Colour (0xff9e8a62), lever.getX(), lever.getBottom(), false));
    g.fillRoundedRectangle (lever, 5.0f);
    g.setColour (juce::Colour (0xff4a3826));
    g.drawRoundedRectangle (lever, 5.0f, 1.0f);
    g.setColour (juce::Colour (0x553b2a1a));
    for (int i = -1; i <= 1; ++i) // grip lines
        g.drawLine (lever.getCentreX() + (float) i * 4.0f, lever.getY() + 6.0f,
                    lever.getCentreX() + (float) i * 4.0f, lever.getBottom() - 6.0f, 1.0f);

    // captions
    g.setFont (fonts->body (11.5f, 0.06f));
    g.setColour (fast ? col::creamDim.withAlpha (0.8f) : col::ink);
    g.drawText ("SLOW", half, juce::Justification::centred);
    g.setColour (fast ? col::ink : col::creamDim.withAlpha (0.8f));
    g.drawText ("FAST", half.translated (half.getWidth(), 0.0f), juce::Justification::centred);
}

// =============================================================================================
// ReelWindow
// =============================================================================================
ReelWindow::ReelWindow (juce::AudioProcessorValueTreeState& apvts)
    : power (apvts.getRawParameterValue ("Powr")), speed (apvts.getRawParameterValue ("TpSp"))
{
    setInterceptsMouseClicks (true, false);
}

void ReelWindow::visibilityChanged()
{
    if (isShowing())
    {
        lastTime = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (30);
    }
    else
        stopTimer();
}

void ReelWindow::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) ((now - lastTime) * 0.001);
    lastTime = now;

    if (power->load() > 0.5f)
    {
        angle += dt * (speed->load() > 0.5f ? 5.0f : 2.4f);
        angle = std::fmod (angle, juce::MathConstants<float>::twoPi);
        repaint();
    }
}

void ReelWindow::drawReel (juce::Graphics& g, juce::Point<float> c, float tapeR, float a) const
{
    // tape pack
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a3020), c.x - tapeR * 0.5f, c.y - tapeR * 0.5f,
                                             juce::Colour (0xff1e120b), c.x + tapeR, c.y + tapeR, true));
    g.fillEllipse (juce::Rectangle<float> (tapeR * 2.0f, tapeR * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0x40ffffff));
    g.drawEllipse (juce::Rectangle<float> (tapeR * 2.0f, tapeR * 2.0f).withCentre (c).reduced (1.5f), 0.8f);

    // hub
    const float hubR = 12.0f;
    g.setColour (juce::Colour (0xffe9dcb8));
    g.fillEllipse (juce::Rectangle<float> (hubR * 2.0f, hubR * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff3a2a1c));
    g.drawEllipse (juce::Rectangle<float> (hubR * 2.0f, hubR * 2.0f).withCentre (c), 1.0f);

    // spokes (dark teeth) - these are what turn
    g.setColour (juce::Colour (0xff3a2a1c));
    for (int i = 0; i < 6; ++i)
    {
        const float s = a + (float) i * juce::MathConstants<float>::twoPi / 6.0f;
        juce::Path tooth;
        tooth.addTriangle (c.getPointOnCircumference (hubR - 1.0f, s - 0.18f),
                           c.getPointOnCircumference (hubR - 1.0f, s + 0.18f),
                           c.getPointOnCircumference (hubR * 0.45f, s));
        g.fillPath (tooth);
    }
    g.setColour (juce::Colour (0xff3a2a1c));
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));
}

void ReelWindow::paint (juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat().reduced (1.0f);

    // metal bezel
    g.setColour (juce::Colour (0xa0000000));
    g.fillRoundedRectangle (outer.translated (0.0f, 2.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7658), outer.getX(), outer.getY(),
                                             juce::Colour (0xff2a1d14), outer.getX(), outer.getBottom(), false));
    g.fillRoundedRectangle (outer, 10.0f);
    g.setColour (juce::Colour (0xff120a06));
    g.drawRoundedRectangle (outer, 10.0f, 1.2f);

    const auto face = outer.reduced (7.0f);
    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle (face, 6.0f);
    g.reduceClipRegion (clip);

    // aged paper face
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1e4c0), face.getCentreX(), face.getCentreY(),
                                             juce::Colour (0xffb89c6a), face.getX(), face.getY(), true));
    g.fillRect (face);

    // faint mounting plate lines + tape path
    const float cy = face.getCentreY() + 2.0f;
    const auto left = juce::Point<float> (face.getX() + face.getWidth() * 0.27f, cy);
    const auto right = juce::Point<float> (face.getRight() - face.getWidth() * 0.27f, cy);

    g.setColour (juce::Colour (0xff2a1a11));
    juce::Path tape;
    tape.startNewSubPath (left.x, left.y + 21.0f);
    tape.quadraticTo (face.getCentreX(), left.y + 31.0f, right.x, right.y + 21.0f);
    g.strokePath (tape, juce::PathStrokeType (2.2f));

    drawReel (g, left, 21.0f, angle);
    drawReel (g, right, 17.0f, angle * 1.15f);

    g.setColour (col::ink.withAlpha (0.75f));
    g.setFont (fonts->body (10.0f, 0.18f));
    g.drawText ("RAINY TAPE  -  SIDE A", face.withHeight (16.0f).translated (0.0f, 2.0f), juce::Justification::centred);

    // glass glare
    juce::Path glare;
    glare.startNewSubPath (face.getX(), face.getY());
    glare.lineTo (face.getX() + face.getWidth() * 0.55f, face.getY());
    glare.lineTo (face.getX() + face.getWidth() * 0.25f, face.getBottom());
    glare.lineTo (face.getX(), face.getBottom());
    glare.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x38ffffff), face.getX(), face.getY(),
                                             juce::Colour (0x00ffffff), face.getX() + 90.0f, face.getY() + 60.0f, false));
    g.fillPath (glare);
    g.restoreState();

    // inner lip
    g.setColour (juce::Colour (0x90000000));
    g.drawRoundedRectangle (face, 6.0f, 1.4f);
}

// =============================================================================================
// PanelBackground
// =============================================================================================
PanelBackground::PanelBackground (juce::Image p, juce::Image w)
    : paintImage (std::move (p)), wearImage (std::move (w))
{
    setOpaque (true);
    setBufferedToImage (true);
    setInterceptsMouseClicks (false, false);
}

void PanelBackground::addHeading (const juce::String& en, const juce::String& jp, juce::Rectangle<float> area)
{
    headings.push_back ({ en, jp, area });
}

void PanelBackground::drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                                        const juce::Font& font, juce::Justification j, juce::Colour c) const
{
    g.setFont (font);
    g.setColour (juce::Colour (0xaa150b06));
    g.drawText (text, area.translated (0.0f, 1.2f), j);
    g.setColour (c);
    g.drawText (text, area, j);
}

void PanelBackground::drawGroove (juce::Graphics& g, juce::Line<float> l, bool horizontal)
{
    const auto off = horizontal ? juce::Point<float> (0.0f, 1.0f) : juce::Point<float> (1.0f, 0.0f);
    g.setColour (juce::Colour (0xcc1a0f09));
    g.drawLine (l, 1.2f);
    g.setColour (juce::Colour (0x50f0dcb0));
    g.drawLine (juce::Line<float> (l.getStart() + off, l.getEnd() + off), 1.0f);
}

void PanelBackground::drawSparkle (juce::Graphics& g, juce::Point<float> c, float r)
{
    juce::Path p;
    const float k = r * 0.22f;
    p.startNewSubPath (c.x, c.y - r);
    p.quadraticTo (c.x + k, c.y - k, c.x + r, c.y);
    p.quadraticTo (c.x + k, c.y + k, c.x, c.y + r);
    p.quadraticTo (c.x - k, c.y + k, c.x - r, c.y);
    p.quadraticTo (c.x - k, c.y - k, c.x, c.y - r);
    p.closeSubPath();
    g.setColour (juce::Colour (0xaa150b06));
    g.fillPath (p, juce::AffineTransform::translation (0.0f, 1.2f));
    g.setColour (col::cream);
    g.fillPath (p);
}

void PanelBackground::drawScrew (juce::Graphics& g, juce::Point<float> c, float angle)
{
    const float R = 8.0f;
    g.setColour (juce::Colour (0xa0000000));
    g.fillEllipse (juce::Rectangle<float> (R * 2.0f + 3.0f, R * 2.0f + 3.0f).withCentre (c.translated (0.0f, 1.2f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9ccb0), c.x - R * 0.5f, c.y - R * 0.6f,
                                             juce::Colour (0xff5d5245), c.x + R, c.y + R, true));
    g.fillEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff1c130d));
    g.drawEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 1.0f);

    const auto a = c.getPointOnCircumference (R - 2.0f, angle);
    const auto b = c.getPointOnCircumference (R - 2.0f, angle + juce::MathConstants<float>::pi);
    g.setColour (juce::Colour (0xff1c130d));
    g.drawLine ({ a, b }, 2.4f);
    g.setColour (juce::Colour (0x50ffffff));
    g.drawLine ({ a.translated (0.6f, 0.9f), b.translated (0.6f, 0.9f) }, 0.8f);
}

void PanelBackground::drawTitle (juce::Graphics& g) const
{
    const auto titleFont = fonts->script (82.0f);
    const juce::String title ("Rainy Tape");
    const juce::Rectangle<float> area (40.0f, 8.0f, 420.0f, 110.0f);

    juce::GlyphArrangement ga;
    ga.addLineOfText (titleFont, title, 0.0f, 0.0f);
    const float tw = ga.getBoundingBox (0, -1, true).getWidth();

    // faded ribbon swoosh under the title
    juce::Path ribbon;
    const float rx0 = area.getX() + 6.0f, rx1 = area.getX() + tw - 8.0f;
    ribbon.startNewSubPath (rx0, 117.0f);
    ribbon.quadraticTo ((rx0 + rx1) * 0.5f, 100.0f, rx1, 84.0f);
    ribbon.quadraticTo ((rx0 + rx1) * 0.5f, 114.0f, rx0, 117.0f);
    ribbon.closeSubPath();
    g.setColour (juce::Colour (0x88150b06));
    g.fillPath (ribbon, juce::AffineTransform::translation (0.0f, 1.5f));
    g.setColour (col::ribbon);
    g.fillPath (ribbon);

    // title: dark relief + cream face
    g.setFont (titleFont);
    g.setColour (juce::Colour (0xb0150b06));
    g.drawText (title, area.translated (2.5f, 3.0f), juce::Justification::centredLeft, false);
    g.setColour (juce::Colour (0xff6e4326));
    g.drawText (title, area.translated (1.0f, 1.2f), juce::Justification::centredLeft, false);
    g.setColour (col::cream);
    g.drawText (title, area, juce::Justification::centredLeft, false);

    drawSparkle (g, { area.getX() + tw + 26.0f, 50.0f }, 11.0f);
    drawSparkle (g, { area.getX() + tw + 12.0f, 82.0f }, 5.5f);

    drawEngravedText (g, "TAPE MODULATION  &  LO-FI", { 48.0f, 120.0f, 340.0f, 18.0f },
                      fonts->body (11.5f, 0.2f), juce::Justification::centredLeft, col::creamDim);
}

void PanelBackground::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (paintImage, bounds, juce::RectanglePlacement::stretchToFit);

    // rust and chipped paint sit under the printed text...
    g.drawImage (wearImage, bounds, juce::RectanglePlacement::stretchToFit);

    // screws
    drawScrew (g, { 28.0f, 28.0f }, 0.5f);
    drawScrew (g, { bounds.getRight() - 28.0f, 28.0f }, 2.4f);
    drawScrew (g, { 28.0f, bounds.getBottom() - 28.0f }, 1.1f);
    drawScrew (g, { bounds.getRight() - 28.0f, bounds.getBottom() - 28.0f }, 2.0f);

    drawTitle (g);

    for (auto& v : vGrooves) drawGroove (g, { v.pos, v.from, v.pos, v.to }, false);
    for (auto& h : hGrooves) drawGroove (g, { h.from, h.pos, h.to, h.pos }, true);

    for (auto& h : headings)
    {
        const auto enFont = fonts->body (13.0f, 0.22f);
        const auto jpFont = fonts->body (11.5f, 0.1f);
        juce::GlyphArrangement ga;
        ga.addLineOfText (enFont, h.en, 0.0f, 0.0f);
        const float ew = ga.getBoundingBox (0, -1, true).getWidth();
        juce::GlyphArrangement gb;
        gb.addLineOfText (jpFont, h.jp, 0.0f, 0.0f);
        const float jw = gb.getBoundingBox (0, -1, true).getWidth();

        auto a = h.area;
        drawEngravedText (g, h.en, a.withWidth (ew + 4.0f), enFont, juce::Justification::centredLeft, col::cream);
        drawEngravedText (g, h.jp, a.withTrimmedLeft (ew + 14.0f).withWidth (jw + 4.0f), jpFont,
                          juce::Justification::centredLeft, col::creamDim);

        const float x0 = a.getX() + ew + jw + 30.0f;
        if (x0 < a.getRight() - 6.0f)
            drawGroove (g, { x0, a.getCentreY() + 1.0f, a.getRight() - 6.0f, a.getCentreY() + 1.0f }, true);
    }

    for (auto& l : labels)
    {
        drawEngravedText (g, l.text, l.area, fonts->body (l.size, l.tracking), l.just, l.colour);
    }

    // ...and a faint second pass over the lettering so it looks worn, yet stays readable.
    g.setOpacity (0.28f);
    g.drawImage (wearImage, bounds, juce::RectanglePlacement::stretchToFit);
}
} // namespace rainy
