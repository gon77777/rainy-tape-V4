#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/TapeEngine.h"
#include "Presets.h"

// TapeLofi: generic tape modulation / lo-fi effect.
//
// Cubase note (see docs/CUBASE_NOTES.md): the host-facing program API is
// deliberately inert. getNumPrograms() returns 1 and setCurrentProgram()
// does nothing, so JUCE's VST3 wrapper never exposes a "Program"
// parameter and Cubase can never overwrite the knob values by calling
// setCurrentProgram() on focus changes or instance creation.
// Presets, when added, must live inside the plugin (loadPreset()) and
// the state must round-trip through APVTS only.
class TapeLofiProcessor final : public juce::AudioProcessor
{
public:
    TapeLofiProcessor();
    ~TapeLofiProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rainy Tape"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    // Host program API: intentionally inert (see header comment).
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Presets: plugin-internal, UI only (never called by the host path) -------------------
    // The preset index is deliberately not part of the saved state; see Presets.h.
    static int getNumPresets() noexcept { return tapelofi::kNumPresets; }
    static const tapelofi::Preset& getPreset (int index) noexcept { return tapelofi::kPresets[index]; }
    void loadPreset (int index);          // writes every sound parameter (not Power) through the host
    int findMatchingPreset() const;       // index whose values equal the current knobs, or -1

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    tapelofi::Engine engine;
    std::atomic<float>* pPower = nullptr;
    std::atomic<float>* pWowDepth = nullptr;
    std::atomic<float>* pWowSpeed = nullptr;
    std::atomic<float>* pFlutterDepth = nullptr;
    std::atomic<float>* pFlutterSpeed = nullptr;
    std::atomic<float>* pDirt = nullptr;
    std::atomic<float>* pAge = nullptr;
    std::atomic<float>* pNoise = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pLowCutQ = nullptr;
    std::atomic<float>* pHighCut = nullptr;
    std::atomic<float>* pHighCutQ = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pLevel = nullptr;
    std::atomic<float>* pTapeSpeed = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeLofiProcessor)
};
