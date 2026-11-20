#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/RainyLookAndFeel.h"
#include "ui/PanelTexture.h"
#include "ui/Widgets.h"
#include "ui/PresetTabs.h"

// Rainy Tape editor: worn brown hardware panel, rotary knobs bound to the APVTS,
// bundled fonts and Japanese tooltips. Host automation / state restore are unchanged
// (everything goes through attachments).
class TapeLofiEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TapeLofiEditor (TapeLofiProcessor& p);
    ~TapeLofiEditor() override;

    void resized() override {}

private:
    // Textures are generated once and shared by every open editor instance.
    struct PanelImageCache
    {
        PanelImageCache() : images (rainy::makePanelImages (rainy::kWidth, rainy::kHeight, 2)) {}
        rainy::PanelImages images;
    };

    void addKnob (const char* paramId, const juce::String& name, rainy::ValueKind kind,
                  const juce::String& tooltip, juce::Rectangle<int> bounds);

    TapeLofiProcessor& processor;

    rainy::RainyLookAndFeel laf;
    juce::SharedResourcePointer<PanelImageCache> imageCache;

    std::unique_ptr<rainy::PanelBackground> background;
    std::vector<std::unique_ptr<rainy::KnobCell>> knobs;
    std::unique_ptr<rainy::ReelWindow> reels;
    rainy::PowerLamp powerLamp;
    rainy::SpeedSwitch speedSwitch;
    std::unique_ptr<rainy::PresetTabs> presetTabs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment, speedAttachment;

    juce::TooltipWindow tooltipWindow { this, 450 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeLofiEditor)
};
