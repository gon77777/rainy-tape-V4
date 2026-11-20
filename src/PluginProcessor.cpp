#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
juce::NormalisableRange<float> logRange (float minHz, float maxHz, float centreHz)
{
    juce::NormalisableRange<float> r (minHz, maxHz, 0.0f);
    r.setSkewForCentre (centreHz);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout TapeLofiProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // IDs and ranges/defaults follow the recovered reference spec (ch. 2).
    params.push_back (std::make_unique<C> (juce::ParameterID { "Powr", 1 }, "Power",
                                           juce::StringArray { "Off", "On" }, 1));
    params.push_back (std::make_unique<P> (juce::ParameterID { "WowP", 1 }, "Wow Depth",
                                           juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "WwSp", 1 }, "Wow Speed",
                                           juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "Fltt", 1 }, "Flutter Depth",
                                           juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "FlSp", 1 }, "Flutter Speed",
                                           juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "Dirt", 1 }, "Dirt",
                                           juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "AgeP", 1 }, "Age",
                                           juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "Nois", 1 }, "Noise",
                                           juce::NormalisableRange<float> (-120.0f, -60.0f, 0.1f), -120.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("dB")));
    // Tapered frequency parameters: skew approximates the reference's
    // 11-point geometric taper tables (Low: 10 Hz..1 kHz, High: 1..35 kHz).
    params.push_back (std::make_unique<P> (juce::ParameterID { "LoCt", 1 }, "Low Cut Freq.",
                                           logRange (10.0f, 1000.0f, 100.0f), 10.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "LoCQ", 1 }, "Low Cut Q",
                                           juce::NormalisableRange<float> (0.7071f, 1.4142f, 0.001f), 1.0607f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "HiCt", 1 }, "High Cut Freq.",
                                           logRange (1000.0f, 35000.0f, 5916.0f), 35000.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "HiCQ", 1 }, "High Cut Q",
                                           juce::NormalisableRange<float> (0.7071f, 1.4142f, 0.001f), 1.0607f));
    params.push_back (std::make_unique<P> (juce::ParameterID { "MixP", 1 }, "Mix",
                                           juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("%")));
    params.push_back (std::make_unique<P> (juce::ParameterID { "Levl", 1 }, "Level",
                                           juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                                           juce::AudioParameterFloatAttributes().withLabel ("dB")));
    params.push_back (std::make_unique<C> (juce::ParameterID { "TpSp", 1 }, "Tape Speed",
                                           juce::StringArray { "Slow", "Fast" }, 1));
    return { params.begin(), params.end() };
}

TapeLofiProcessor::TapeLofiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    pPower = apvts.getRawParameterValue ("Powr");
    pWowDepth = apvts.getRawParameterValue ("WowP");
    pWowSpeed = apvts.getRawParameterValue ("WwSp");
    pFlutterDepth = apvts.getRawParameterValue ("Fltt");
    pFlutterSpeed = apvts.getRawParameterValue ("FlSp");
    pDirt = apvts.getRawParameterValue ("Dirt");
    pAge = apvts.getRawParameterValue ("AgeP");
    pNoise = apvts.getRawParameterValue ("Nois");
    pLowCut = apvts.getRawParameterValue ("LoCt");
    pLowCutQ = apvts.getRawParameterValue ("LoCQ");
    pHighCut = apvts.getRawParameterValue ("HiCt");
    pHighCutQ = apvts.getRawParameterValue ("HiCQ");
    pMix = apvts.getRawParameterValue ("MixP");
    pLevel = apvts.getRawParameterValue ("Levl");
    pTapeSpeed = apvts.getRawParameterValue ("TpSp");
}

TapeLofiProcessor::~TapeLofiProcessor() = default;

void TapeLofiProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, getTotalNumInputChannels());
    setLatencySamples (engine.latencySamples());
}

void TapeLofiProcessor::releaseResources() {}

bool TapeLofiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TapeLofiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    tapelofi::Parameters p;
    p.power = pPower->load() > 0.5f;
    p.wowDepth = pWowDepth->load();
    p.wowSpeed = pWowSpeed->load();
    p.flutterDepth = pFlutterDepth->load();
    p.flutterSpeed = pFlutterSpeed->load();
    p.dirt = pDirt->load();
    p.age = pAge->load();
    p.noiseDb = pNoise->load();
    p.lowCutHz = pLowCut->load();
    p.lowCutQ = pLowCutQ->load();
    p.highCutHz = pHighCut->load();
    p.highCutQ = pHighCutQ->load();
    p.mix = pMix->load();
    p.levelDb = pLevel->load();
    p.tapeSpeed = (int) pTapeSpeed->load();
    engine.setParameters (p);

    const int nCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    float* ptrs[2] = { nullptr, nullptr };
    for (int ch = 0; ch < nCh && ch < 2; ++ch) ptrs[ch] = buffer.getWritePointer (ch);
    engine.process (ptrs, juce::jmin (nCh, 2), n);
}

juce::AudioProcessorEditor* TapeLofiProcessor::createEditor()
{
    return new TapeLofiEditor (*this);
}

void TapeLofiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeLofiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore parameter values only. Never route state restore through a
    // program/preset loader (see docs/CUBASE_NOTES.md).
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

namespace
{
// Parameter id -> value of that parameter inside a preset. Power is intentionally absent.
struct PresetEntry { const char* id; float value; };

std::array<PresetEntry, 14> presetEntries (const tapelofi::Preset& p)
{
    return { { { "WowP", p.wowDepth },    { "WwSp", p.wowSpeed },
               { "Fltt", p.flutterDepth }, { "FlSp", p.flutterSpeed },
               { "Dirt", p.dirt },         { "AgeP", p.age },
               { "Nois", p.noiseDb },
               { "LoCt", p.lowCutHz },     { "LoCQ", p.lowCutQ },
               { "HiCt", p.highCutHz },    { "HiCQ", p.highCutQ },
               { "MixP", p.mix },          { "Levl", p.levelDb },
               { "TpSp", (float) p.tapeSpeed } } };
}
} // namespace

void TapeLofiProcessor::loadPreset (int index)
{
    if (index < 0 || index >= getNumPresets())
        return;

    for (const auto& e : presetEntries (getPreset (index)))
    {
        if (auto* param = apvts.getParameter (e.id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (e.value));
            param->endChangeGesture();
        }
    }
}

int TapeLofiProcessor::findMatchingPreset() const
{
    // Compare in the normalised 0..1 domain: parameters snap to their step size, so a loaded
    // preset can differ from the table by up to half a step. 0.0012 is a little over one step
    // of the finest-grained parameter and far below the gap between two different presets.
    constexpr float tolerance = 0.0012f;

    for (int i = 0; i < getNumPresets(); ++i)
    {
        bool same = true;
        for (const auto& e : presetEntries (getPreset (i)))
        {
            auto* param = apvts.getParameter (e.id);
            if (param == nullptr || std::abs (param->getValue() - param->convertTo0to1 (e.value)) > tolerance)
            {
                same = false;
                break;
            }
        }
        if (same)
            return i;
    }
    return -1;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeLofiProcessor();
}
