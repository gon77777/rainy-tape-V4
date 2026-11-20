#include "RainyLookAndFeel.h"

namespace rainy
{
namespace
{
constexpr float kTooltipWidth = 270.0f;

juce::ColourGradient radial (juce::Colour inner, juce::Colour outer, juce::Point<float> centre, float radius)
{
    return juce::ColourGradient (inner, centre, outer, centre.translated (radius, 0.0f), true);
}
} // namespace

RainyLookAndFeel::RainyLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, col::amberText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::textColourId, col::ink);
}

juce::Font RainyLookAndFeel::getLabelFont (juce::Label&)
{
    return fonts->body (13.0f);
}

juce::Label* RainyLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->getProperties().set ("plate", true);
    l->setJustificationType (juce::Justification::centred);
    l->setFont (fonts->body (13.0f));
    l->setMinimumHorizontalScale (0.8f);
    l->setColour (juce::Label::textColourId, col::amberText);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textWhenEditingColourId, col::cream);
    l->setColour (juce::Label::backgroundWhenEditingColourId, col::plate);
    l->setColour (juce::Label::outlineWhenEditingColourId, col::amber);
    return l;
}

void RainyLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (! label.getProperties().contains ("plate"))
    {
        LookAndFeel_V4::drawLabel (g, label);
        return;
    }

    auto r = label.getLocalBounds().toFloat().reduced (0.5f);

    // Recessed plate: dark well, light lip along the bottom edge.
    g.setColour (juce::Colour (0x30f0dcb0));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 4.0f);
    g.setColour (col::plate);
    g.fillRoundedRectangle (r, 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x80000000), r.getX(), r.getY(),
                                             juce::Colour (0x00000000), r.getX(), r.getY() + 8.0f, false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff0e0805));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    if (! label.isBeingEdited())
    {
        g.setColour (label.findColour (juce::Label::textColourId));
        g.setFont (getLabelFont (label));
        g.drawFittedText (label.getText(), label.getLocalBounds().reduced (3, 0),
                          juce::Justification::centred, 1, 0.8f);
    }
}

void RainyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();
    const float tickR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
    const float baseR = tickR - 11.0f;       // dark skirt of the knob
    const float capR  = baseR * 0.80f;       // bone-coloured cap

    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroPos = bipolar ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
    const float litLo = juce::jmin (sliderPos, zeroPos);
    const float litHi = juce::jmax (sliderPos, zeroPos);

    // ---- scale ticks ------------------------------------------------------------
    constexpr int numTicks = 11;
    for (int i = 0; i < numTicks; ++i)
    {
        const float pos = (float) i / (float) (numTicks - 1);
        const float a = startAngle + pos * (endAngle - startAngle);
        const bool end = (i == 0 || i == numTicks - 1 || (bipolar && i == numTicks / 2));
        const bool lit = pos >= litLo - 0.001f && pos <= litHi + 0.001f && (bipolar ? pos != zeroPos || i == numTicks / 2 : true);
        const auto p = centre.getPointOnCircumference (tickR - 2.0f, a);
        const float s = end ? 2.5f : 1.7f;

        if (lit)
        {
            g.setColour (juce::Colour (0x40ffb53f));
            g.fillEllipse (juce::Rectangle<float> (s * 3.2f, s * 3.2f).withCentre (p));
        }
        g.setColour (lit ? col::amberText : col::creamDim.withAlpha (0.55f));
        if (end)
            g.fillRect (juce::Rectangle<float> (s * 1.6f, s * 1.6f).withCentre (p));
        else
            g.fillEllipse (juce::Rectangle<float> (s, s).withCentre (p));
    }

    // ---- drop shadow ---------------------------------------------------------------
    {
        const auto sc = centre.translated (1.5f, 4.0f);
        auto grad = juce::ColourGradient (juce::Colour (0xb0000000), sc, juce::Colour (0x00000000),
                                          sc.translated (baseR + 8.0f, 0.0f), true);
        grad.addColour ((double) baseR / (double) (baseR + 8.0f), juce::Colour (0x90000000));
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> ((baseR + 8.0f) * 2.0f, (baseR + 8.0f) * 2.0f).withCentre (sc));
    }

    // ---- skirt (dark bakelite ring with fine grip grooves) ---------------------------
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b5646), centre.x, centre.y - baseR,
                                                 juce::Colour (0xff17100b), centre.x, centre.y + baseR, false));
        g.fillEllipse (juce::Rectangle<float> (baseR * 2.0f, baseR * 2.0f).withCentre (centre));
        g.setColour (juce::Colour (0xff0b0604));
        g.drawEllipse (juce::Rectangle<float> (baseR * 2.0f, baseR * 2.0f).withCentre (centre), 1.2f);

        g.setColour (juce::Colour (0x38000000));
        for (int i = 0; i < 48; ++i)
        {
            const float a = (float) i / 48.0f * juce::MathConstants<float>::twoPi;
            g.drawLine (juce::Line<float> (centre.getPointOnCircumference (baseR - 0.5f, a),
                                           centre.getPointOnCircumference (baseR - 4.5f, a)), 0.9f);
        }
    }

    // ---- cap (aged bone / bakelite) ------------------------------------------------------
    {
        const auto capBox = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
        // soft shadow ring between skirt and cap
        g.setColour (juce::Colour (0x90000000));
        g.fillEllipse (capBox.expanded (1.6f));

        auto grad = juce::ColourGradient (juce::Colour (0xfff3e7c8), centre.x - capR * 0.35f, centre.y - capR * 0.45f,
                                          juce::Colour (0xff9b8660), centre.x + capR * 0.9f, centre.y + capR * 1.0f, true);
        grad.addColour (0.55, juce::Colour (0xffd5c39b));
        g.setGradientFill (grad);
        g.fillEllipse (capBox);

        // age spots (deterministic per knob size)
        juce::Random rnd (4242);
        for (int i = 0; i < 38; ++i)
        {
            const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
            const float d = std::sqrt (rnd.nextFloat()) * capR * 0.95f;
            const auto p = centre.getPointOnCircumference (d, a);
            const float s = 0.6f + rnd.nextFloat() * 1.8f;
            g.setColour (juce::Colour (rnd.nextFloat() < 0.7f ? 0xff6a4a2c : 0xffa88a5c)
                             .withAlpha (0.10f + rnd.nextFloat() * 0.22f));
            g.fillEllipse (juce::Rectangle<float> (s, s).withCentre (p));
        }

        // inner dished ring + rim
        g.setColour (juce::Colour (0x38ffffff));
        g.drawEllipse (capBox.reduced (capR * 0.14f), 1.0f);
        g.setColour (juce::Colour (0x40000000));
        g.drawEllipse (capBox.reduced (capR * 0.14f).translated (0.0f, 1.0f), 1.0f);
        g.setColour (juce::Colour (0xff4a3826));
        g.drawEllipse (capBox, 1.1f);
    }

    // ---- pointer ----------------------------------------------------------------------------
    {
        juce::Path p;
        const float len = capR * 0.80f;
        const float w = juce::jmax (2.4f, capR * 0.085f);
        p.addRoundedRectangle (-w * 0.5f, -len, w, len * 0.78f, w * 0.5f);
        const auto rot = juce::AffineTransform::rotation (angle).translated (centre.x, centre.y);

        g.setColour (juce::Colour (0x66fff3d8));
        g.fillPath (p, rot.translated (0.9f, 0.9f));
        g.setColour (juce::Colour (0xff2b1a11));
        g.fillPath (p, rot);
    }
}

// ---------------------------------------------------------------------------------------------
void RainyLookAndFeel::drawLamp (juce::Graphics& g, juce::Rectangle<float> area, bool on)
{
    const auto c = area.getCentre();
    const float R = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float bezelR = R * 0.62f;
    const float glassR = bezelR * 0.78f;

    if (on)
    {
        auto halo = juce::ColourGradient (juce::Colour (0x70ffa21f), c, juce::Colour (0x00ffa21f), c.translated (R, 0.0f), true);
        g.setGradientFill (halo);
        g.fillEllipse (area);
    }

    // shadow + bezel
    g.setColour (juce::Colour (0x80000000));
    g.fillEllipse (juce::Rectangle<float> (bezelR * 2.0f, bezelR * 2.0f).withCentre (c.translated (0.0f, 2.0f)).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb5a283), c.x, c.y - bezelR,
                                             juce::Colour (0xff3a2d22), c.x, c.y + bezelR, false));
    g.fillEllipse (juce::Rectangle<float> (bezelR * 2.0f, bezelR * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff1a110b));
    g.drawEllipse (juce::Rectangle<float> (bezelR * 2.0f, bezelR * 2.0f).withCentre (c), 1.0f);

    // glass
    const auto glass = juce::Rectangle<float> (glassR * 2.0f, glassR * 2.0f).withCentre (c);
    auto grad = on ? juce::ColourGradient (juce::Colour (0xffffeaa0), c.x - glassR * 0.2f, c.y - glassR * 0.25f,
                                           juce::Colour (0xffb34a08), c.x + glassR, c.y + glassR, true)
                   : juce::ColourGradient (juce::Colour (0xff6a3412), c.x - glassR * 0.2f, c.y - glassR * 0.25f,
                                           juce::Colour (0xff1f0d06), c.x + glassR, c.y + glassR, true);
    if (on) grad.addColour (0.45, juce::Colour (0xffffa21f));
    g.setGradientFill (grad);
    g.fillEllipse (glass);

    // specular highlight
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.28f));
    g.fillEllipse (juce::Rectangle<float> (glassR * 0.75f, glassR * 0.45f).withCentre (c.translated (-glassR * 0.28f, -glassR * 0.46f)));
}

// ---------------------------------------------------------------------------------------------
juce::TextLayout RainyLookAndFeel::tooltipLayout (const juce::String& text) const
{
    juce::AttributedString s;
    s.setJustification (juce::Justification::topLeft);
    s.setLineSpacing (3.0f);
    s.append (text, fonts->body (13.0f), col::ink);
    juce::TextLayout layout;
    layout.createLayout (s, kTooltipWidth);
    return layout;
}

juce::Rectangle<int> RainyLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                         juce::Rectangle<int> parentArea)
{
    const auto layout = tooltipLayout (tipText);
    const int w = (int) std::ceil (layout.getWidth()) + 24;
    const int h = (int) std::ceil (layout.getHeight()) + 18;

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
                                 w, h)
        .constrainedWithin (parentArea);
}

void RainyLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> ((float) width, (float) height);

    // aged paper label with a dark-brown frame
    g.setColour (juce::Colour (0xff2b1a11));
    g.fillRoundedRectangle (r, 5.0f);
    auto inner = r.reduced (1.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff5e9c6), inner.getX(), inner.getY(),
                                             juce::Colour (0xffdcc698), inner.getRight(), inner.getBottom(), false));
    g.fillRoundedRectangle (inner, 4.0f);

    juce::Random rnd (99);
    for (int i = 0; i < 26; ++i)
    {
        const float s = 0.8f + rnd.nextFloat() * 1.6f;
        g.setColour (juce::Colour (0xff7a5632).withAlpha (0.08f + rnd.nextFloat() * 0.14f));
        g.fillEllipse (juce::Rectangle<float> (s, s).withCentre ({ inner.getX() + rnd.nextFloat() * inner.getWidth(),
                                                                    inner.getY() + rnd.nextFloat() * inner.getHeight() }));
    }

    tooltipLayout (text).draw (g, r.reduced (12.0f, 9.0f));
}
} // namespace rainy
