#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// Rainy Tape: shared fonts and colour palette.
namespace rainy
{
// Bundled fonts (see assets/fonts). Lifetime is tied to the editors that use it
// (SharedResourcePointer), so nothing is torn down at DLL unload time.
//   body   : Zen Antique (subset: kana, ASCII, JIS level-1 kanji)  - SIL OFL 1.1
//   script : Yellowtail (Latin only, used for the title)           - Apache 2.0
struct FontCache
{
    FontCache()
        : bodyFace (juce::Typeface::createSystemTypefaceFor (BinaryData::ZenAntiqueSubset_ttf,
                                                             (size_t) BinaryData::ZenAntiqueSubset_ttfSize)),
          scriptFace (juce::Typeface::createSystemTypefaceFor (BinaryData::Yellowtail_ttf,
                                                               (size_t) BinaryData::Yellowtail_ttfSize))
    {
    }

    juce::Font body (float height, float tracking = 0.0f) const
    {
        return juce::Font (juce::FontOptions (bodyFace).withHeight (height)).withExtraKerningFactor (tracking);
    }

    juce::Font script (float height) const
    {
        return juce::Font (juce::FontOptions (scriptFace).withHeight (height));
    }

    juce::Typeface::Ptr bodyFace, scriptFace;
};

namespace col
{
    const juce::Colour cream     { 0xffeedfba };
    const juce::Colour creamDim  { 0xffb9a37b };
    const juce::Colour ink       { 0xff2b1a11 };
    const juce::Colour plate     { 0xff251610 };
    const juce::Colour rust      { 0xffb4542b };
    const juce::Colour ribbon    { 0xffbb5a40 };
    const juce::Colour amber     { 0xffffb53f };
    const juce::Colour amberText { 0xfff6d9a2 };
    const juce::Colour paper     { 0xfff0e2bd };
}

// Panel geometry shared by the background renderer and the editor layout.
constexpr int kWidth      = 900;
constexpr int kMainHeight = 566;                        // knobs + header (unchanged from V2)
constexpr int kShelfTop   = kMainHeight;                // preset shelf sits strictly below it
constexpr int kShelfHeight = 120;
constexpr int kHeight     = kMainHeight + kShelfHeight; // whole panel
} // namespace rainy
