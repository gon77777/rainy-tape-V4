#include "TapeEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace tapelofi
{
namespace
{

constexpr double kPi = 3.14159265358979323846;

double dbToLin (double db) { return std::pow (10.0, db / 20.0); }

// ---------------------------------------------------------------- RBJ biquad
// Type numbering follows the reference product's designer (F1281):
// 5 = low-pass, 6 = high-pass, 12 = peaking (gain/Q carrying type).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0.0; }

    void design (int type, double fs, double freq, double gainDb, double q)
    {
        q = std::max (q, 0.1);
        freq = std::clamp (freq, 5.0, fs * 0.49);
        const double w = 2.0 * kPi * freq / fs;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
        switch (type)
        {
            case 5: // low-pass
                B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2;
                A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha;
                break;
            case 6: // high-pass
                B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2;
                A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha;
                break;
            default: // 12: peaking
            {
                const double A = std::pow (10.0, gainDb / 40.0);
                B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
                A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A;
                break;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// ------------------------------------------------- windowed-sinc resampling
// Blackman-windowed sinc, 32 taps per phase (mirrors the reference kernels).
// Group delay is 16 host-rate samples in each direction, at any factor,
// so total latency stays constant across host sample rates.
struct Resampler
{
    int factor = 1;
    std::vector<double> upKernel;   // index = k*factor + phase
    std::vector<double> downKernel; // index = tap
    std::vector<double> inHist;     // mirrored host-rate history (2 x 33)
    std::vector<double> osHist;     // mirrored internal-rate history (2 x len)
    int inPos = 0, osPos = 0, osCount = 0;
    int inLen = 33, osLen = 33;
    double downPower = 1.0;         // sum of squared down-kernel taps (white-noise power gain)

    static double sinc (double x) { return std::abs (x) < 1e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x); }
    static double blackman (double t) // t in [-1, 1]
    {
        return 0.42 + 0.5 * std::cos (kPi * t) + 0.08 * std::cos (2.0 * kPi * t);
    }

    void prepare (int f)
    {
        factor = f;
        upKernel.assign ((size_t) 33 * f, 0.0);
        for (int i = 0; i < 33 * f; ++i)
        {
            const double t = (double) i / f - 16.0; // host-sample units, spans [-16, 17)
            upKernel[(size_t) i] = 0.995 * sinc (0.995 * t) * blackman (t / 16.0);
        }
        const int len = 32 * f + 1;
        downKernel.assign ((size_t) len, 0.0);
        for (int i = 0; i < len; ++i)
        {
            const double t = (double) (i - 16 * f); // internal-sample units
            downKernel[(size_t) i] = (0.95 / f) * sinc (0.95 * t / f) * blackman (t / (16.0 * f));
        }
        downPower = 0.0;
        for (double c : downKernel) downPower += c * c;
        inLen = 33;
        osLen = len;
        inHist.assign ((size_t) inLen * 2, 0.0);
        osHist.assign ((size_t) osLen * 2, 0.0);
        reset();
    }

    void reset() noexcept
    {
        std::fill (inHist.begin(), inHist.end(), 0.0);
        std::fill (osHist.begin(), osHist.end(), 0.0);
        inPos = osPos = osCount = 0;
    }

    // Push one host-rate sample; writes `factor` internal-rate samples.
    // The history is stored twice (pos and pos+len) so the newest-to-oldest
    // walk below is a plain contiguous read with no wrap-around handling.
    void up (double x, double* out) noexcept
    {
        inHist[(size_t) inPos] = x;
        inHist[(size_t) (inPos + inLen)] = x;
        const double* h = inHist.data() + inPos + inLen; // h[-k] = sample k steps old
        const double* kern = upKernel.data();
        for (int p = 0; p < factor; ++p)
        {
            double acc = 0.0;
            for (int k = 0; k < inLen; ++k)
                acc += h[-k] * kern[k * factor + p];
            out[p] = acc;
        }
        if (++inPos >= inLen) inPos = 0;
    }

    // Push one internal-rate sample; returns true (and a value) every
    // `factor` pushes.
    bool down (double x, double& out) noexcept
    {
        osHist[(size_t) osPos] = x;
        osHist[(size_t) (osPos + osLen)] = x;
        const int newest = osPos + osLen;
        if (++osPos >= osLen) osPos = 0;
        if (++osCount < factor) return false;
        osCount = 0;
        const double* h = osHist.data() + newest; // h[-j] = sample j steps old
        const double* kern = downKernel.data();
        double acc = 0.0;
        for (int j = 0; j < osLen; ++j)
            acc += h[-j] * kern[j];
        out = acc;
        return true;
    }
};

// ---------------------------------------------------------------- delay line
struct DelayLine
{
    std::vector<double> buf;
    int pos = 0;

    void prepare (int size) { buf.assign ((size_t) std::max (size, 16), 0.0); pos = 0; }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0); pos = 0; }

    inline void push (double x) noexcept
    {
        buf[(size_t) pos] = x;
        if (++pos >= (int) buf.size()) pos = 0;
    }

    inline double at (int i) const noexcept
    {
        const int n = (int) buf.size();
        if (i < 0) i += n; else if (i >= n) i -= n;
        return buf[(size_t) i];
    }

    // Fractional read, 4-point cubic Hermite (spec 4.4 default). The caller
    // keeps delaySamples within [2, size-4].
    inline double read (double delaySamples) const noexcept
    {
        const int n = (int) buf.size();
        double idx = (double) pos - 1.0 - delaySamples;
        if (idx < 0.0) idx += n;
        const int i1 = (int) idx;
        const double f = idx - i1;
        const double y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
        const double c0 = y1;
        const double c1 = 0.5 * (y2 - y0);
        const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
};

// ---------------------------------------------------------------------- RNG
// Per-instance xorshift (the reference used a global one; we do not).
struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    void seed (uint64_t v) noexcept { s = v ? v : 0x9E3779B97F4A7C15ull; }
    inline uint64_t next() noexcept { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    inline double uniform() noexcept { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    // Approximate gaussian: sum of 10 uniforms (same trick as the reference).
    inline double gaussian() noexcept
    {
        double a = 0.0;
        for (int i = 0; i < 10; ++i) a += uniform();
        return (a - 5.0) * 0.5477; // RMS ~= 0.5
    }
};

// ------------------------------------------------------------------ smoother
struct Smoother
{
    double cur = 0, tgt = 0, coeff = 0;
    void prepare (double fs, double tauSeconds) noexcept
    {
        coeff = 1.0 - std::exp (-1.0 / (tauSeconds * fs));
    }
    void snap (double v) noexcept { cur = tgt = v; }
    void setTarget (double v) noexcept { tgt = v; }
    inline double next() noexcept { cur += coeff * (tgt - cur); return cur; }
};

// ------------------------------------------------------------ saturation
// Bounded rational soft clipper, x(27+x^2)/(27+9x^2) (a tanh approximation,
// same family as the reference). The expression is only a tanh stand-in for
// |x| < 3, where it reaches exactly +-1 with zero slope; beyond that it must
// be held at +-1. (V1 evaluated it unclamped, where it tends to x/9, i.e. it
// stopped clipping and started attenuating.)
inline double ratClip (double y) noexcept
{
    y = std::clamp (y, -3.0, 3.0);
    return y * (27.0 + y * y) / (27.0 + 9.0 * y * y);
}

// One saturation stage. Small-signal gain is exactly 1 for any drive (the
// output is divided by the drive), and `kHeadroom` keeps drive = 1 almost
// transparent. Larger drives bend loud material toward a ceiling of
// kHeadroom / drive instead of changing quiet material.
constexpr double kHeadroom = 2.0;
inline double saturate (double x, double drive) noexcept
{
    return kHeadroom * ratClip (x * drive / kHeadroom) / drive;
}

// Drive law (own calibration, see docs/TUNING.md). The spec only fixes that
// Age^3 drives the saturator; the factors below were fitted so that a
// 1 kHz / -6 dBFS sine loses ~1.4 dB at Dirt 100 (Probe: -1.4 dB) and ~3 dB
// at Age 100 from saturation alone. The rest of Probe's -4.9 dB at Age 100 is
// left to the Age darkening filter, because Probe's test signal was
// broadband, not a sine.
constexpr double kDirtDrive = 2.1;
constexpr double kAgeDrive  = 4.0;
constexpr double kStage2Share = 0.5; // second stage gets half the drive

// Age darkening one-pole: open at Age 0, 4.5 kHz at Age 100.
constexpr double kDarkenOpenHz = 40000.0, kDarkenClosedHz = 4500.0;

// Modulation: Age widens wow/flutter by up to +30 %, and the sum of all
// excursions must stay inside the +-1.8 ms the 4 ms line allows around the
// 2 ms centre ((1.1 + 0.22 + 0.03) * 1.3 = 1.755 ms).
constexpr double kWowDepthSec = 0.0011, kFlutDepthSec = 0.00022, kHumDepthSec = 0.00003;
constexpr double kAgeDepthBoost = 0.3;

// LFO shaping: mild rounding of the sine peaks (non-sinusoidal, as the
// reference flutter), peak 0.955.
inline double lfoShape (double x) noexcept { return kHeadroom * 1.25 * ratClip (x / (kHeadroom * 1.25)); }

// Tape EQ cascade tables, recovered from the reference (spec v0.7, D-03).
// Order: [type], [gain dB], [Q], [freq Hz] for Slow / Fast.
constexpr int    kEqType[6]    = { 6, 12, 12, 12, 12, 5 };
constexpr double kEqGainSlow[6] = { 0.0, -1.51, 1.72, -0.93, 0.43, 0.0 };
constexpr double kEqGainFast[6] = { 0.0, -1.21, 2.13, -1.23, 0.41, 0.0 };
constexpr double kEqQSlow[6]    = { 0.93, 1.50, 1.21, 1.21, 0.41, 0.75 };
constexpr double kEqQFast[6]    = { 0.95, 1.21, 1.31, 1.23, 0.53, 0.80 };
constexpr double kEqFreqSlow[6] = { 12.47, 23.75, 43.38, 62.30, 1300.0, 12750.0 };
constexpr double kEqFreqFast[6] = { 19.51, 51.21, 82.37, 127.30, 1620.0, 25500.0 };

// Base oscillator frequencies of the reference (Hz).
constexpr double kWowFreq = 3.63, kFlutterFreqA = 13.57, kFlutterFreqB = 21.17, kHumFreq = 50.0;

constexpr double kTapeSpeedFadeSec = 0.025;

// Noise knob calibration (measured, see tests/EngineTest.cpp).
constexpr double kNoiseCal = 2.0;

} // namespace

// --------------------------------------------------------------------- Impl
struct Engine::Impl
{
    double hostRate = 44100, osRate = 176400;
    int factor = 4;
    int numChannels = 2;

    Resampler resampler[2];
    DelayLine delay[2];
    Biquad cascade[2][6];
    Biquad cascadeOld[2][6]; // outgoing EQ during a Tape Speed crossfade
    Biquad lowCut[2], highCut[2];
    Rng rng[2];

    int fadeLeft = 0, fadeLen = 1;

    // Per-channel one-pole states.
    double noiseHpX[2] = {}, noiseHpY[2] = {};
    double crackleEnv[2] = {};
    double agePole[2] = {};

    // LFO phases (shared across channels, like the reference).
    double phWow = 0, phFlutA = 0, phFlutB = 0, phHum = 0;

    Smoother sWowDepth, sFlutDepth, sDirt, sAge, sNoiseDb, sMix, sLevelDb, sPower;

    int curTapeSpeed = -1;
    double curLoHz = -1, curLoQ = -1, curHiHz = -1, curHiQ = -1;

    void prepareSmoothers()
    {
        sWowDepth.prepare (osRate, 0.050);
        sFlutDepth.prepare (osRate, 0.050);
        sDirt.prepare (osRate, 0.050);
        sAge.prepare (osRate, 0.050);
        sNoiseDb.prepare (osRate, 0.050);
        sMix.prepare (osRate, 0.020);
        sLevelDb.prepare (osRate, 0.020);
        sPower.prepare (osRate, 0.010);
    }

    void designCascade (int speed, bool crossfade)
    {
        if (crossfade)
        {
            // Keep the outgoing filters (with their state) running and fade to
            // the new set, so switching Tape Speed does not click.
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < 6; ++i)
                    cascadeOld[ch][i] = cascade[ch][i];
            fadeLen = std::max (1, (int) (kTapeSpeedFadeSec * osRate));
            fadeLeft = fadeLen;
        }
        const auto* g = speed == 0 ? kEqGainSlow : kEqGainFast;
        const auto* q = speed == 0 ? kEqQSlow : kEqQFast;
        const auto* f = speed == 0 ? kEqFreqSlow : kEqFreqFast;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < 6; ++i)
                cascade[ch][i].design (kEqType[i], osRate, f[i], g[i], q[i]);
        curTapeSpeed = speed;
    }

    void designCuts (double loHz, double loQ, double hiHz, double hiQ)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            lowCut[ch].design (6, osRate, loHz, 0.0, loQ);
            highCut[ch].design (5, osRate, hiHz, 0.0, hiQ);
        }
        curLoHz = loHz; curLoQ = loQ; curHiHz = hiHz; curHiQ = hiQ;
    }
};

Engine::Engine() : impl (new Impl) {}
Engine::~Engine() { delete impl; }

void Engine::prepare (double hostSampleRate, int numChannels)
{
    auto& im = *impl;
    im.hostRate = hostSampleRate;
    im.numChannels = std::clamp (numChannels, 1, 2);
    im.factor = hostSampleRate < 50000.0 ? 4 : (hostSampleRate < 100000.0 ? 2 : 1);
    im.osRate = hostSampleRate * im.factor;
    factor = im.factor;
    internalRate = im.osRate;

    for (int ch = 0; ch < im.numChannels; ++ch)
    {
        im.resampler[ch].prepare (im.factor);
        im.delay[ch].prepare ((int) std::ceil (im.osRate * 0.004) + 8); // 4 ms, as the reference
        im.rng[ch].seed (0x2545F4914F6CDD1Dull + (uint64_t) ch * 0x9E3779B97F4A7C15ull);
    }
    im.prepareSmoothers();
    im.designCascade (target.tapeSpeed, false);
    im.designCuts (target.lowCutHz, target.lowCutQ, target.highCutHz, target.highCutQ);

    // Latency: 16 (up) + 16 (down) host samples + a whole number of host
    // samples for the 2 ms centre delay (so the reported value is exact at
    // every sample rate, e.g. 44.1 kHz).
    centreHostSamples = (int) std::llround (0.002 * hostSampleRate);
    latency = 32 + centreHostSamples;
    reset();

    // Snap smoothers to the current targets so prepare() is click-free.
    im.sWowDepth.snap (target.wowDepth / 100.0);
    im.sFlutDepth.snap (target.flutterDepth / 100.0);
    im.sDirt.snap (target.dirt / 100.0);
    im.sAge.snap (target.age / 100.0);
    im.sNoiseDb.snap (target.noiseDb);
    im.sMix.snap (target.mix / 100.0);
    im.sLevelDb.snap (target.levelDb);
    im.sPower.snap (target.power ? 1.0 : 0.0);
}

void Engine::reset()
{
    auto& im = *impl;
    for (int ch = 0; ch < im.numChannels; ++ch)
    {
        im.resampler[ch].reset();
        im.delay[ch].reset();
        for (auto& b : im.cascade[ch]) b.reset();
        for (auto& b : im.cascadeOld[ch]) b.reset();
        im.lowCut[ch].reset();
        im.highCut[ch].reset();
        im.noiseHpX[ch] = im.noiseHpY[ch] = 0.0;
        im.crackleEnv[ch] = 0.0;
        im.agePole[ch] = 0.0;
    }
    im.fadeLeft = 0;
    im.phWow = im.phFlutA = im.phFlutB = im.phHum = 0.0;
}

void Engine::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    auto& im = *impl;
    // Never touch more channels than prepare() set up or than the host gave us.
    int nCh = std::min (im.numChannels, numChannels);
    for (int ch = 0; ch < nCh; ++ch)
        if (channels[ch] == nullptr) { nCh = ch; break; }
    if (nCh <= 0 || numSamples <= 0) return;

    const double osRate = im.osRate;

    // Parameter targets for this block.
    im.sWowDepth.setTarget (target.wowDepth / 100.0);
    im.sFlutDepth.setTarget (target.flutterDepth / 100.0);
    im.sDirt.setTarget (target.dirt / 100.0);
    im.sAge.setTarget (target.age / 100.0);
    im.sNoiseDb.setTarget (target.noiseDb);
    im.sMix.setTarget (target.mix / 100.0);
    im.sLevelDb.setTarget (target.levelDb);
    im.sPower.setTarget (target.power ? 1.0 : 0.0);
    if (target.tapeSpeed != im.curTapeSpeed) im.designCascade (target.tapeSpeed, true);
    if (std::abs (target.lowCutHz - im.curLoHz) > 0.01 || std::abs (target.lowCutQ - im.curLoQ) > 0.001
        || std::abs (target.highCutHz - im.curHiHz) > 0.5 || std::abs (target.highCutQ - im.curHiQ) > 0.001)
        im.designCuts (target.lowCutHz, target.lowCutQ, target.highCutHz, target.highCutQ);

    // Speed multipliers: bipolar -100..+100 maps to x0.5 .. x2 (one octave).
    const double wowMul = std::pow (2.0, target.wowSpeed / 100.0);
    const double flutMul = std::pow (2.0, target.flutterSpeed / 100.0);
    const double incWow = kWowFreq * wowMul / osRate;
    const double incFlutA = kFlutterFreqA * flutMul / osRate;
    const double incFlutB = kFlutterFreqB * flutMul / osRate;
    const double incHum = kHumFreq / osRate;

    const double centerDelay = (double) centreHostSamples * im.factor; // integer, in internal samples
    const double minDelay = 0.0001 * osRate, maxDelay = 0.0039 * osRate;
    const double hpCoeff = std::exp (-2.0 * kPi * 400.0 / osRate); // one-pole HP coefficient
    // White hiss at the internal rate is band-limited by the down-sampler.
    // Its exact power gain is the sum of the squared kernel taps, so dividing
    // by that makes the Noise knob read as the RMS level of the hiss at the
    // output, in dBFS, at any host rate (the generator itself has RMS 0.5).
    const double noiseScale = kNoiseCal / std::sqrt (im.resampler[0].downPower);
    const double crackleDecay = std::exp (-1.0 / (0.0005 * osRate));
    const double darkenMaxHz = std::min (kDarkenOpenHz, 0.45 * osRate);

    double upBuf[2][4];
    double outSample[2] = { 0.0, 0.0 };

    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < nCh; ++ch)
            im.resampler[ch].up ((double) channels[ch][n], upBuf[ch]);

        for (int p = 0; p < im.factor; ++p)
        {
            // ---- smoothed parameters (per internal sample) ----
            const double wowD = im.sWowDepth.next();
            const double flutD = im.sFlutDepth.next();
            const double dirt = im.sDirt.next();
            const double age = im.sAge.next();
            const double age3 = age * age * age;
            const double noiseGain = dbToLin (im.sNoiseDb.next());
            const double mix = im.sMix.next();
            const double level = dbToLin (im.sLevelDb.next());
            const double power = im.sPower.next();

            // ---- LFOs ----
            im.phWow += incWow;     if (im.phWow >= 1.0) im.phWow -= 1.0;
            im.phFlutA += incFlutA; if (im.phFlutA >= 1.0) im.phFlutA -= 1.0;
            im.phFlutB += incFlutB; if (im.phFlutB >= 1.0) im.phFlutB -= 1.0;
            im.phHum += incHum;     if (im.phHum >= 1.0) im.phHum -= 1.0;
            const double wowShape = lfoShape (std::sin (2.0 * kPi * im.phWow));
            const double flutShape = lfoShape (0.6 * std::sin (2.0 * kPi * im.phFlutA)
                                               + 0.4 * std::sin (2.0 * kPi * im.phFlutB));
            const double hum = std::sin (2.0 * kPi * im.phHum);

            const double depthScale = 1.0 + kAgeDepthBoost * age3; // Age deepens modulation
            const double delaySec = 0.002
                + wowD * kWowDepthSec * wowShape * depthScale
                + flutD * (kFlutDepthSec * flutShape + kHumDepthSec * hum) * depthScale;
            const double delaySamples = std::clamp (delaySec * osRate, minDelay, maxDelay);

            // ---- saturation drive from Dirt + Age^3 ----
            const double drive1 = 1.0 + dirt * kDirtDrive + age3 * kAgeDrive;
            const double drive2 = 1.0 + kStage2Share * (drive1 - 1.0);
            const double ageFc = darkenMaxHz * std::pow (kDarkenClosedHz / darkenMaxHz, age * age);
            const double ageCoeff = 1.0 - std::exp (-2.0 * kPi * ageFc / osRate);
            const double crackleP = 2.0e-5 * (0.25 + dirt * 2.0 + age);

            // Tape Speed crossfade weight (1 = new set).
            double fadeW = 1.0;
            if (im.fadeLeft > 0)
            {
                fadeW = 1.0 - (double) im.fadeLeft / (double) im.fadeLen;
                --im.fadeLeft;
            }
            const bool fading = fadeW < 1.0;

            for (int ch = 0; ch < nCh; ++ch)
            {
                const double in = upBuf[ch][p];
                im.delay[ch].push (in);
                const double dry = im.delay[ch].read (centerDelay);
                double x = im.delay[ch].read (delaySamples);

                // Tape EQ cascade (6 stages).
                if (fading)
                {
                    double xo = x;
                    for (int i = 0; i < 6; ++i) xo = im.cascadeOld[ch][i].process (xo);
                    for (int i = 0; i < 6; ++i) x = im.cascade[ch][i].process (x);
                    x = xo + (x - xo) * fadeW;
                }
                else
                {
                    for (int i = 0; i < 6; ++i) x = im.cascade[ch][i].process (x);
                }

                // Noise: shaped gaussian hiss + sparse crackle impulses.
                const double g = im.rng[ch].gaussian();
                const double hp = hpCoeff * (im.noiseHpY[ch] + g - im.noiseHpX[ch]);
                im.noiseHpX[ch] = g; im.noiseHpY[ch] = hp;
                if (im.rng[ch].uniform() < crackleP)
                    im.crackleEnv[ch] = (im.rng[ch].uniform() * 2.0 - 1.0);
                im.crackleEnv[ch] *= crackleDecay;
                x += (hp + im.crackleEnv[ch] * 0.8) * noiseGain * noiseScale;

                // Saturation, two stages.
                x = saturate (x, drive1);
                x = saturate (x, drive2);

                // Age darkening (one-pole low-pass, Hz-based so it sounds the
                // same at every host rate).
                im.agePole[ch] += ageCoeff * (x - im.agePole[ch]);
                x = im.agePole[ch];

                // Output cuts.
                x = im.lowCut[ch].process (x);
                x = im.highCut[ch].process (x);

                // Output stage: Power gate -> Mix -> Level, all against the
                // latency-matched dry signal.
                const double wet = dry + (x - dry) * power;
                const double y = (dry * (1.0 - mix) + wet * mix) * level;

                double out = 0.0;
                if (im.resampler[ch].down (y, out)) outSample[ch] = out;
            }
        }

        for (int ch = 0; ch < nCh; ++ch)
            channels[ch][n] = (float) outSample[ch];
    }
}

} // namespace tapelofi
