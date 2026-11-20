#include "PluginEditor.h"

using rainy::ValueKind;

namespace
{
// Tooltips are Japanese. Only kana and common kanji that exist in the bundled font are used.
const juce::String kReset = juce::String::fromUTF8 ("\n（ダブルクリックで初期値にもどります）");

juce::String tip (const char* utf8, bool withReset = true)
{
    return juce::String::fromUTF8 (utf8) + (withReset ? kReset : juce::String());
}
} // namespace

TapeLofiEditor::TapeLofiEditor (TapeLofiProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    using namespace rainy;
    setLookAndFeel (&laf);
    setOpaque (true);

    // ---- layout constants -------------------------------------------------------------------
    constexpr int left = 44, colW = 116;
    constexpr int row1Y = 184, row1H = 158;
    constexpr int row2Y = 372, row2H = 180;
    constexpr int groupSplitX = left + colW * 4;   // between 4-knob groups and the rest
    constexpr int right = left + colW * 7;

    background = std::make_unique<PanelBackground> (imageCache->images.paint, imageCache->images.wear);
    addAndMakeVisible (*background);

    // ---- static labels / grooves ------------------------------------------------------------------
    background->addHeading ("WOBBLE", juce::String::fromUTF8 ("ゆれ"),
                            { (float) left + 6.0f, 158.0f, (float) (groupSplitX - left) - 14.0f, 22.0f });
    background->addHeading ("TAPE", juce::String::fromUTF8 ("テープ"),
                            { (float) groupSplitX + 14.0f, 158.0f, (float) (right - groupSplitX) - 20.0f, 22.0f });
    background->addHeading ("FILTER", juce::String::fromUTF8 ("フィルター"),
                            { (float) left + 6.0f, 346.0f, (float) (groupSplitX - left) - 14.0f, 22.0f });
    background->addHeading ("OUTPUT", juce::String::fromUTF8 ("出力"),
                            { (float) groupSplitX + 14.0f, 346.0f, (float) (right - groupSplitX) - 20.0f, 22.0f });

    background->addHorizontalGroove (150.0f, 32.0f, (float) kWidth - 32.0f);
    background->addVerticalGroove ((float) groupSplitX, 164.0f, 548.0f);

    // ---- preset shelf (own strip below the knob panel: nothing here overlaps the controls above) ----
    background->addHorizontalGroove ((float) kShelfTop - 4.0f, 32.0f, (float) kWidth - 32.0f);
    background->addLabel ({ "PRESET", { (float) left + 6.0f, (float) kShelfTop + 14.0f, 100.0f, 20.0f }, 13.0f, 0.22f,
                            juce::Justification::centredLeft, col::cream });
    background->addLabel ({ juce::String::fromUTF8 ("プリセット"), { (float) left + 6.0f, (float) kShelfTop + 33.0f, 100.0f, 18.0f },
                            11.5f, 0.1f, juce::Justification::centredLeft, col::creamDim });

    background->addLabel ({ "TAPE SPEED", { 686.0f, 38.0f, 104.0f, 18.0f }, 11.5f, 0.16f,
                            juce::Justification::centred, col::creamDim });
    background->addLabel ({ "POWER", { 790.0f, 110.0f, 80.0f, 18.0f }, 11.5f, 0.16f,
                            juce::Justification::centred, col::creamDim });

    // ---- knobs --------------------------------------------------------------------------------------
    auto cell = [&] (int col, int row) -> juce::Rectangle<int>
    {
        return { left + col * colW, row == 0 ? row1Y : row2Y + (row2H - row1H) / 2, colW, row1H };
    };

    addKnob ("WowP", "Wow Depth", ValueKind::percent,
             tip ("ゆっくりした音程のゆれ（ワウ）の深さです。\n大きいほど、テープがよれたような音になります。"), cell (0, 0));
    addKnob ("WwSp", "Wow Speed", ValueKind::signedPercent,
             tip ("ワウのゆれる速さです。\n左にまわすとゆっくり、右にまわすと速くなります。"), cell (1, 0));
    addKnob ("Fltt", "Flutter Depth", ValueKind::percent,
             tip ("細かくて速い音程のゆれ（フラッター）の深さです。\n大きいほど、音がふるえます。"), cell (2, 0));
    addKnob ("FlSp", "Flutter Speed", ValueKind::signedPercent,
             tip ("フラッターのゆれる速さです。\n左でゆっくり、右で速くなります。"), cell (3, 0));

    addKnob ("Dirt", "Dirt", ValueKind::percent,
             tip ("テープのよごれ具合です。\n大きいほど、音がざらついてひずみます。"), cell (4, 0));
    addKnob ("AgeP", "Age", ValueKind::percent,
             tip ("テープの古さです。\n大きいほど、ひずみが増えて高い音がこもっていきます。"), cell (5, 0));
    addKnob ("Nois", "Noise", ValueKind::noiseDb,
             tip ("テープのヒスノイズとパチパチ音の量です。\nいちばん左でオフになります。"), cell (6, 0));

    addKnob ("LoCt", "Low Cut", ValueKind::hertz,
             tip ("ここより低い音をカットします。\n右にまわすほど、低い音がうすくなります。"), cell (0, 1));
    addKnob ("LoCQ", "Low Cut Q", ValueKind::plain,
             tip ("ローカットの切れぎわのくせです。\n大きいと、切れぎわの音がもりあがります。"), cell (1, 1));
    addKnob ("HiCt", "High Cut", ValueKind::hertz,
             tip ("ここより高い音をカットします。\n左にまわすほど、音がこもります。"), cell (2, 1));
    addKnob ("HiCQ", "High Cut Q", ValueKind::plain,
             tip ("ハイカットの切れぎわのくせです。\n大きいと、切れぎわの音がもりあがります。"), cell (3, 1));

    // Output: two larger knobs sharing the last three columns.
    const int outW = (right - groupSplitX) / 2;
    addKnob ("MixP", "Mix", ValueKind::percent,
             tip ("元の音とエフェクト音のバランスです。\n左で元の音だけ、右でエフェクト音だけになります。"),
             { groupSplitX, row2Y, outW, row2H });
    addKnob ("Levl", "Level", ValueKind::decibel,
             tip ("出力の音量です。\n-18 dB から +18 dB まで調整できます。"),
             { groupSplitX + outW, row2Y, outW, row2H });

    // ---- header: reels, tape speed, power ---------------------------------------------------------
    reels = std::make_unique<ReelWindow> (processor.apvts);
    reels->setTooltip (tip ("テープのリールです。\nパワーがオンのあいだ、まわります。", false));
    reels->setBounds (452, 26, 214, 100);
    addAndMakeVisible (*reels);

    speedSwitch.setTooltip (tip ("テープの速さを切りかえます。\nSLOWはやわらかくこもった音、FASTは明るくはっきりした音になります。", false));
    speedSwitch.setBounds (690, 62, 96, 40);
    addAndMakeVisible (speedSwitch);
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "TpSp", speedSwitch);

    powerLamp.setTooltip (tip ("パワースイッチです。\nオフにするとエフェクトがかからず、音がそのまま通ります。", false));
    powerLamp.setBounds (800, 44, 60, 60);
    addAndMakeVisible (powerLamp);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "Powr", powerLamp);

    // ---- preset tabs (UI-only; see docs/CUBASE_NOTES.md) -------------------------------------------
    presetTabs = std::make_unique<PresetTabs> (processor);
    presetTabs->setBounds (left, kShelfTop + 14, right - left, 86);   // x: 44..856
    addAndMakeVisible (*presetTabs);

    // Slider text boxes were created before the look-and-feel was reachable; rebuild them.
    sendLookAndFeelChange();

    setResizable (false, false);
    setSize (kWidth, kHeight);
    background->setBounds (getLocalBounds());
    background->toBack();
}

TapeLofiEditor::~TapeLofiEditor()
{
    setLookAndFeel (nullptr);
}

void TapeLofiEditor::addKnob (const char* paramId, const juce::String& name, rainy::ValueKind kind,
                              const juce::String& tooltip, juce::Rectangle<int> bounds)
{
    auto k = std::make_unique<rainy::KnobCell> (processor.apvts, paramId, kind, tooltip);
    k->setBounds (bounds);
    addAndMakeVisible (*k);
    knobs.push_back (std::move (k));

    background->addLabel ({ name, bounds.toFloat().withHeight ((float) rainy::kNameStripHeight).translated (0.0f, 2.0f),
                            13.5f, 0.05f, juce::Justification::centred, rainy::col::cream });
}
