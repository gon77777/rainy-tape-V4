#pragma once

// TapeEngine: framework-independent tape modulation / lo-fi DSP core.
//
// Design follows the engine spec (P002_tape_lofi_engine_spec.md, ch. 3-4):
//   input -> oversampling up -> modulated delay (wow/flutter)
//         -> 6-stage tape EQ cascade (Slow/Fast tables)
//         -> noise add -> saturation x2 (Dirt + Age^3 driven) -> age darkening
//         -> Low Cut -> High Cut -> output blend (Power/Mix/Level)
//         -> oversampling down -> output
//
// - All processing runs at an internal rate of roughly 192 kHz
//   (host < 50 kHz: x4, < 100 kHz: x2, otherwise x1), like the reference.
// - Latency is constant: the bypass path goes through the same resamplers
//   and the same delay line, so Power on/off never changes the delay.
// - The random source is per-instance (never global).
// - Parameters are smoothed per-sample inside the engine, so the sound
//   does not depend on the host block size.

#include <cstdint>

namespace tapelofi
{

struct Parameters
{
    bool  power        = true;
    float wowDepth     = 0.0f;    // 0..100 %
    float wowSpeed     = 0.0f;    // -100..+100 % (bipolar, 0 = base speed)
    float flutterDepth = 0.0f;    // 0..100 %
    float flutterSpeed = 0.0f;    // -100..+100 %
    float dirt         = 20.0f;   // 0..100 %
    float age          = 10.0f;   // 0..100 %
    float noiseDb      = -120.0f; // -120..-60 dB
    float lowCutHz     = 10.0f;   // 10..1000 Hz
    float lowCutQ      = 1.06066f;
    float highCutHz    = 35000.0f; // 1000..35000 Hz
    float highCutQ     = 1.06066f;
    float mix          = 100.0f;  // 0..100 %
    float levelDb      = 0.0f;    // -18..+18 dB
    int   tapeSpeed    = 1;       // 0 = Slow, 1 = Fast
};

class Engine
{
public:
    Engine();
    ~Engine();

    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    // Call from prepareToPlay. numChannels is 1 (mono) or 2 (stereo).
    void prepare (double hostSampleRate, int numChannels);
    void reset();

    // Reported latency in host-rate samples. Valid after prepare().
    int latencySamples() const noexcept { return latency; }

    int oversamplingFactor() const noexcept { return factor; }
    double internalSampleRate() const noexcept { return internalRate; }

    // Cheap to call every block; targets are smoothed internally.
    void setParameters (const Parameters& p) noexcept { target = p; }

    // In-place processing. channels[ch] must hold numSamples samples.
    // numChannels is what the host actually handed over; the engine never
    // touches more channels than that or than prepare() was given, and it
    // skips null pointers. Works for any block size, including 1.
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

private:
    struct Impl;
    Impl* impl = nullptr;
    Parameters target;
    int latency = 0;
    int centreHostSamples = 88;
    int factor = 1;
    double internalRate = 0.0;
};

} // namespace tapelofi
