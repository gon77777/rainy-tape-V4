#pragma once

// Factory presets for Rainy Tape. Plain data, no JUCE dependency (unit-tested in tests/EngineTest.cpp).
//
// Rules (see docs/CUBASE_NOTES.md):
//  - Presets are applied only by TapeLofiProcessor::loadPreset(), called from the editor.
//    The host-facing program API stays inert.
//  - The preset index is NOT stored in the plugin state. The editor works out which preset
//    (if any) matches the current knob values, so nothing needs restoring.
//  - Power is not part of a preset: choosing a sound never switches the effect off.
//
// Strings are UTF-8. Kanji are limited to what the bundled Zen Antique subset contains.

namespace tapelofi
{
struct Preset
{
    const char* nameJp;    // shown big on the tab
    const char* nameEn;    // shown small on the tab
    const char* tipJp;     // tooltip (reading + one-line description)

    float wowDepth, wowSpeed, flutterDepth, flutterSpeed;   // %, %, %, %
    float dirt, age;                                        // %
    float noiseDb;                                          // dB
    float lowCutHz, lowCutQ, highCutHz, highCutQ;
    float mix;                                              // %
    float levelDb;                                          // dB
    int   tapeSpeed;                                        // 0 = Slow, 1 = Fast
};

inline constexpr float kQ = 1.0607f; // factory-default filter Q

inline constexpr Preset kPresets[] =
{
    //  JP            EN                 tooltip
    //  wow  wSp  flut fSp  dirt age  noise  loHz  loQ    hiHz   hiQ   mix  lvl spd
    { "小雨",     "Drizzle",           "小雨（こさめ）\nぱらぱらとふる、かるい雨。\nほんのりテープの味がつきます。",
       10,   0,   6,   0,  18, 18,  -96,   20, kQ,  16000, kQ,  100,  0.0f, 1 },

    { "霧雨",     "Mist",              "霧雨（きりさめ）\n白くけむる、しっとりした雨。\n高い音がやわらかく丸くなります。",
       14, -25,   4, -20,  15, 30,  -92,   20, kQ,   7500, kQ,  100,  1.0f, 0 },

    { "春雨",     "Spring Rain",       "春雨（はるさめ）\nあたたかくて、おだやかな雨。\nゆったりしたゆれが心地よい音です。",
       24, -20,   8,   0,  25, 24,  -94,   30, kQ,  11000, 0.85f, 100,  0.0f, 1 },

    { "五月雨",   "Early Summer Rain", "五月雨（さみだれ）\nいつまでもふりつづく、つゆどきの雨。\nゆれとノイズがずっとつきまといます。",
       32,  10,  20,  15,  38, 36,  -86,   70, kQ,   8500, kQ,  100,  1.0f, 1 },

    { "夕立",     "Evening Shower",    "夕立（ゆうだち）\nとつぜんふりだす、はげしい雨。\nひずみとノイズが強めの、あらっぽい音です。",
       38,  25,  38,  35,  62, 45,  -78,  140, 1.2f,  9500, 1.2f, 100, -1.0f, 1 },

    { "時雨",     "Passing Shower",    "時雨（しぐれ）\nふったり、やんだりをくりかえす雨。\n大きなゆれで、音程がふらふらします。",
       58, -45,  16, -10,  30, 42,  -90,   40, kQ,   5200, 1.1f, 100,  1.5f, 0 },

    { "雨宿り",   "Taking Shelter",    "雨宿り（あまやどり）\n軒下できく、くぐもった雨の音。\n高い音をおさえた、おちついた音です。",
       20, -35,   6, -20,  30, 55,  -88,   90, kQ,   3200, 1.0f, 100,  2.0f, 0 },

    { "梅雨",     "Rainy Season",      "梅雨（つゆ）\nじめじめとしめった、重たい空気。\n古びたテープのような、くすんだ音です。",
       46, -10,  26,   5,  52, 72,  -80,   60, kQ,   4200, kQ,  100,  2.0f, 0 },

    { "氷雨",     "Cold Rain",         "氷雨（ひさめ）\nつめたく、ほそい雨。\n低い音がうすく、きんとした冷たい音です。",
       18,  20,  34,  40,  22, 22,  -84,  240, 1.35f, 6200, 1.3f, 100,  0.0f, 1 },

    { "雨音",     "Raindrops",         "雨音（あまおと）\nまどをたたく、ひとつぶひとつぶの音。\nパチパチというノイズを、はっきり聞かせます。",
        4,   0,   2,   0,  20, 22,  -70,   30, kQ,  12000, kQ,  100, -1.0f, 1 },

    { "雨上がり", "After the Rain",    "雨上がり（あめあがり）\nやんだあとの、すんだ空気。\n元の音をのこした、ごく軽いかけ方です。",
        7,  15,   3,  10,  10,  8, -108,   20, kQ,  20000, kQ,   70,  0.0f, 1 },

    { "豪雨",     "Downpour",          "豪雨（ごうう）\nたたきつけるような、はげしい雨。\nすべてを強くかけた、荒れた音です。",
       72,  35,  62,  50,  92, 86,  -66,  180, 1.3f,  5600, 1.25f, 100, -2.0f, 0 },
};

inline constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
} // namespace tapelofi
