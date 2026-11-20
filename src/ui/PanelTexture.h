#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rainy
{
// Procedurally generated, deterministic textures (no image files needed).
//   paint : opaque painted-metal base (brown, mottled, rain streaks, grain)
//   wear  : transparent overlay with chipped paint, rust and scratches,
//           drawn on top of the printed labels so they look worn.
struct PanelImages
{
    juce::Image paint, wear;
};

PanelImages makePanelImages (int logicalWidth, int logicalHeight, int pixelScale);
} // namespace rainy
