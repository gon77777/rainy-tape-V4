// Standalone DSP test: no JUCE required.
//   g++ -std=c++17 -O2 -I src src/dsp/TapeEngine.cpp tests/EngineTest.cpp -o engine_test
#include "dsp/TapeEngine.h"
#include "Presets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

double renderAndMeasure (tapelofi::Engine& e, const tapelofi::Parameters& p,
                         int channels, double seconds, double freq, double amp,
                         bool measureTailHalf)
{
    e.setParameters (p);
    const int sr = 48000;
    const int total = (int) (sr * seconds);
    std::vector<float> buf0 ((size_t) total), buf1 ((size_t) total);
    for (int i = 0; i < total; ++i)
    {
        const float v = (float) (amp * std::sin (2.0 * 3.141592653589793 * freq * i / sr));
        buf0[(size_t) i] = v;
        buf1[(size_t) i] = v;
    }
    float* ptrs[2] = { buf0.data(), buf1.data() };
    // Process in odd-sized blocks to exercise block-size independence.
    int done = 0;
    while (done < total)
    {
        const int n = std::min (137, total - done);
        float* p2[2] = { buf0.data() + done, buf1.data() + done };
        e.process (p2, 2, n);
        done += n;
    }
    (void) ptrs;
    double sum = 0.0;
    int count = 0;
    bool finite = true;
    const int start = measureTailHalf ? total / 2 : 0;
    for (int i = start; i < total; ++i)
    {
        if (! std::isfinite (buf0[(size_t) i])) finite = false;
        sum += (double) buf0[(size_t) i] * buf0[(size_t) i];
        ++count;
    }
    check (finite, "output is finite");
    return std::sqrt (sum / count);
}
} // namespace

int main()
{
    using tapelofi::Engine;
    using tapelofi::Parameters;

    // --- Defaults, stereo, 48 kHz (x4 oversampling) ---
    {
        Engine e;
        e.prepare (48000, 2);
        check (e.oversamplingFactor() == 4, "48 kHz -> x4 oversampling");
        check (e.latencySamples() == 32 + 96, "latency = 32 + 2 ms @48k");
        Parameters p; // defaults: wow/flutter 0, noise off, mix 100
        const double rms = renderAndMeasure (e, p, 2, 1.0, 1000.0, 0.5, true);
        std::printf ("       default RMS = %.4f (expect ~0.33-0.38)\n", rms);
        check (rms > 0.30 && rms < 0.40, "default path passes 1 kHz sine near unity");
    }

    // --- Mono, wow at full depth: must stay finite and sane ---
    {
        Engine e;
        e.prepare (48000, 1);
        Parameters p;
        p.wowDepth = 100.0f; p.flutterDepth = 100.0f;
        const double rms = renderAndMeasure (e, p, 1, 1.0, 3150.0, 0.5, true);
        std::printf ("       wow/flutter full RMS = %.4f\n", rms);
        check (rms > 0.2 && rms < 0.45, "full wow/flutter keeps level sane");
    }

    // --- Noise: silent input, noise at -70 dB must be audible but small ---
    {
        Engine e;
        e.prepare (44100, 2);
        Parameters p;
        p.noiseDb = -70.0f;
        p.mix = 100.0f;
        const double rms = renderAndMeasure (e, p, 2, 1.0, 1000.0, 0.0, true);
        std::printf ("       noise-only RMS = %.5f\n", rms);
        check (rms > 0.00005 && rms < 0.01, "noise floor present at -70 dB setting");
    }

    // --- Power off: passthrough (delayed dry), near-unity level ---
    {
        Engine e;
        e.prepare (48000, 2);
        Parameters p;
        p.power = false;
        p.wowDepth = 100.0f; p.age = 100.0f; // must be ignored when off
        const double rms = renderAndMeasure (e, p, 2, 1.0, 1000.0, 0.5, true);
        std::printf ("       power-off RMS = %.4f\n", rms);
        check (rms > 0.33 && rms < 0.38, "power off is clean passthrough");
    }

    // --- Age full: level drops (reference measured -4.9 dB direction) ---
    {
        Engine e;
        e.prepare (48000, 2);
        Parameters p;
        p.age = 100.0f;
        const double rms = renderAndMeasure (e, p, 2, 1.0, 1000.0, 0.5, true);
        std::printf ("       age=100 RMS = %.4f (expect < default 0.354)\n", rms);
        check (rms < 0.35, "age reduces level (saturation compression)");
    }

    // --- High sample rate: x1 oversampling ---
    {
        Engine e;
        e.prepare (192000, 2);
        check (e.oversamplingFactor() == 1, "192 kHz -> x1 oversampling");
        Parameters p;
        const double rms = renderAndMeasure (e, p, 2, 0.5, 1000.0, 0.5, true);
        check (rms > 0.28 && rms < 0.42, "192 kHz path passes sine");
    }

    // --- Reported latency equals measured latency at several rates.
    //     Measured on the dry path (Power off, and Mix 0 with Power on); the
    //     wet path adds the tape EQ's own group delay on top, as on tape. ---
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int mode = 0; mode < 2; ++mode)
        {
            Engine e;
            Parameters p;
            p.dirt = 0.0f; p.age = 0.0f;
            if (mode == 0) p.power = false; else p.mix = 0.0f;
            e.setParameters (p);   // before prepare(), so the smoothers start settled
            e.prepare (sr, 2);
            std::vector<float> a (4000, 0.0f), b (4000, 0.0f);
            a[100] = b[100] = 1.0f;
            float* pp[2] = { a.data(), b.data() };
            e.process (pp, 2, 4000);
            int pk = 0;
            for (int i = 0; i < 4000; ++i) if (std::fabs (a[(size_t) i]) > std::fabs (a[(size_t) pk])) pk = i;
            char msg[112];
            std::snprintf (msg, sizeof msg, "latency %d reported == measured (sr %.0f, %s)",
                           e.latencySamples(), sr, mode == 0 ? "power off" : "mix 0");
            check (std::abs (pk - 100 - e.latencySamples()) <= 1, msg);
        }

    // --- Noise knob reads its dB label (RMS, dBFS) at every host rate ---
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Engine e;
        e.prepare (sr, 2);
        Parameters p;
        p.noiseDb = -70.0f; p.dirt = 0.0f; p.age = 0.0f;
        e.setParameters (p);
        const int total = (int) (sr * 1.0);
        std::vector<float> a ((size_t) total, 0.0f), b ((size_t) total, 0.0f);
        float* pp[2] = { a.data(), b.data() };
        e.process (pp, 2, total);
        double sum = 0.0;
        for (int i = total / 2; i < total; ++i) sum += (double) a[(size_t) i] * a[(size_t) i];
        const double dbfs = 10.0 * std::log10 (sum / (total - total / 2));
        char msg[96];
        // At 192 kHz the host band reaches 91 kHz, and the default High Cut
        // (35 kHz) removes hiss above 35 kHz, so the broadband RMS reads lower.
        const double tol = sr > 150000.0 ? 4.0 : 1.5;
        std::snprintf (msg, sizeof msg, "noise -70 reads %.1f dBFS at %.0f Hz (within %.1f dB)", dbfs, sr, tol);
        check (std::fabs (dbfs + 70.0) < tol, msg);
    }

    // --- Saturator is bounded: a +12 dBFS sine at maximum drive never explodes,
    //     and louder input never produces a *quieter* output (V1 did) ---
    {
        double lastRms = 0.0;
        bool monotonic = true;
        for (double amp : { 0.1, 0.25, 0.5, 1.0, 2.0, 4.0 })
        {
            Engine e;
            e.prepare (48000, 2);
            Parameters p;
            p.dirt = 100.0f; p.age = 100.0f;
            const double rms = renderAndMeasure (e, p, 2, 0.5, 1000.0, amp, true);
            if (rms < lastRms * 0.98) monotonic = false;
            lastRms = rms;
        }
        check (monotonic, "output level is monotonic in input level at Dirt/Age 100");
    }

    // --- Dirt 100 costs about 1.4 dB on a -6 dBFS sine (Probe: -1.4 dB) ---
    {
        Engine e0, e1;
        e0.prepare (48000, 2); e1.prepare (48000, 2);
        Parameters a, b;
        a.dirt = 0.0f; a.age = 0.0f;
        b.dirt = 100.0f; b.age = 0.0f;
        const double r0 = renderAndMeasure (e0, a, 2, 1.0, 1000.0, 0.5, true);
        const double r1 = renderAndMeasure (e1, b, 2, 1.0, 1000.0, 0.5, true);
        const double d = 20.0 * std::log10 (r1 / r0);
        std::printf ("       dirt 100 vs 0 = %.2f dB\n", d);
        check (d < -0.8 && d > -2.0, "Dirt 100 lowers a -6 dBFS sine by ~1.4 dB");
    }

    // --- Defaults are nearly transparent (Dirt 20 / Age 10: within 0.5 dB) ---
    {
        Engine e;
        e.prepare (48000, 2);
        Parameters p;
        const double rms = renderAndMeasure (e, p, 2, 1.0, 1000.0, 0.5, true);
        check (std::fabs (20.0 * std::log10 (rms / (0.5 / std::sqrt (2.0)))) < 0.5, "factory default is within 0.5 dB of unity");
    }

    // --- Worst-case modulation (Age/Wow/Flutter 100, fastest) stays finite and bounded ---
    {
        Engine e;
        e.prepare (44100, 2);
        Parameters p;
        p.wowDepth = p.flutterDepth = 100.0f; p.wowSpeed = p.flutterSpeed = 100.0f; p.age = 100.0f;
        const double rms = renderAndMeasure (e, p, 2, 2.0, 3150.0, 0.5, true);
        check (rms > 0.05 && rms < 0.5, "worst-case modulation stays sane");
    }

    // --- Tape Speed switch does not click (sample-to-sample jump stays small) ---
    {
        Engine e;
        e.prepare (48000, 2);
        Parameters p;
        p.dirt = 0.0f; p.age = 0.0f; p.tapeSpeed = 1;
        e.setParameters (p);
        const int total = 48000;
        std::vector<float> a ((size_t) total), b ((size_t) total);
        for (int i = 0; i < total; ++i)
            a[(size_t) i] = b[(size_t) i] = (float) (0.3 * std::sin (2.0 * 3.141592653589793 * 220.0 * i / 48000.0));
        int done = 0;
        while (done < total)
        {
            if (done >= 24000) { p.tapeSpeed = 0; e.setParameters (p); }
            const int n = std::min (256, total - done);
            float* pp[2] = { a.data() + done, b.data() + done };
            e.process (pp, 2, n);
            done += n;
        }
        // A 220 Hz / 0.3 sine changes by at most ~0.014 per sample.
        double maxJump = 0.0;
        for (int i = 20000; i < total - 1; ++i)
            maxJump = std::max (maxJump, (double) std::fabs (a[(size_t) i + 1] - a[(size_t) i]));
        std::printf ("       max sample step across speed switch = %.4f\n", maxJump);
        check (maxJump < 0.03, "Tape Speed switch is click-free");
    }

    // --- Block-size independence: block 1 == block 512, bit for bit ---
    {
        Parameters p;
        p.wowDepth = 60.0f; p.flutterDepth = 40.0f; p.age = 40.0f; p.noiseDb = -70.0f;
        const int total = 12000;
        std::vector<float> in ((size_t) total);
        for (int i = 0; i < total; ++i) in[(size_t) i] = (float) (0.3 * std::sin (2.0 * 3.141592653589793 * 440.0 * i / 48000.0));
        auto render = [&] (int block) {
            Engine e; e.prepare (48000, 2); e.setParameters (p);
            std::vector<float> a = in, b = in;
            for (int d = 0; d < total; d += block)
            {
                const int n = std::min (block, total - d);
                float* pp[2] = { a.data() + d, b.data() + d };
                e.process (pp, 2, n);
            }
            return a;
        };
        const auto a = render (1), b = render (512);
        check (a == b, "block size 1 and 512 give identical output");
    }

    // --- Robustness: fewer channels than prepared, null pointers, zero samples ---
    {
        Engine e;
        e.prepare (48000, 2);
        std::vector<float> a (256, 0.1f);
        float* one[2] = { a.data(), nullptr };
        e.process (one, 1, 256);   // host gave 1 channel to a stereo engine
        e.process (one, 2, 256);   // second pointer null
        e.process (one, 2, 0);     // empty block
        check (std::isfinite (a[255]), "mismatched channel count / null pointer is safe");
    }

    // --- Level is a plain gain: +6 dB doubles a -20 dBFS sine ---
    {
        Engine e0, e1;
        e0.prepare (48000, 2); e1.prepare (48000, 2);
        Parameters a, b;
        a.dirt = b.dirt = 0.0f; a.age = b.age = 0.0f;
        b.levelDb = 6.0f;
        const double r0 = renderAndMeasure (e0, a, 2, 1.0, 1000.0, 0.1, true);
        const double r1 = renderAndMeasure (e1, b, 2, 1.0, 1000.0, 0.1, true);
        check (std::fabs (20.0 * std::log10 (r1 / r0) - 6.0) < 0.1, "Level +6 dB is +6 dB");
    }

    // --- Factory presets: in range, unique, and every one renders finite, audible, non-clipping audio ---
    {
        using tapelofi::kPresets;
        using tapelofi::kNumPresets;
        check (kNumPresets == 12, "12 factory presets");

        bool inRange = true, unique = true, finite = true, audible = true, sane = true;
        for (int i = 0; i < kNumPresets; ++i)
        {
            const auto& q = kPresets[i];
            auto in = [] (float v, float lo, float hi) { return v >= lo && v <= hi; };
            inRange = inRange && in (q.wowDepth, 0, 100) && in (q.wowSpeed, -100, 100)
                      && in (q.flutterDepth, 0, 100) && in (q.flutterSpeed, -100, 100)
                      && in (q.dirt, 0, 100) && in (q.age, 0, 100) && in (q.noiseDb, -120, -60)
                      && in (q.lowCutHz, 10, 1000) && in (q.lowCutQ, 0.7071f, 1.4142f)
                      && in (q.highCutHz, 1000, 35000) && in (q.highCutQ, 0.7071f, 1.4142f)
                      && in (q.mix, 0, 100) && in (q.levelDb, -18, 18) && (q.tapeSpeed == 0 || q.tapeSpeed == 1);
            for (int j = i + 1; j < kNumPresets; ++j)
                unique = unique && std::string (q.nameEn) != kPresets[j].nameEn
                                && std::string (q.nameJp) != kPresets[j].nameJp;

            Parameters p;
            p.wowDepth = q.wowDepth; p.wowSpeed = q.wowSpeed;
            p.flutterDepth = q.flutterDepth; p.flutterSpeed = q.flutterSpeed;
            p.dirt = q.dirt; p.age = q.age; p.noiseDb = q.noiseDb;
            p.lowCutHz = q.lowCutHz; p.lowCutQ = q.lowCutQ;
            p.highCutHz = q.highCutHz; p.highCutQ = q.highCutQ;
            p.mix = q.mix; p.levelDb = q.levelDb; p.tapeSpeed = q.tapeSpeed;

            Engine e;
            e.prepare (48000, 2);
            const double rms = renderAndMeasure (e, p, 2, 1.0, 440.0, 0.25, true);
            finite = finite && std::isfinite (rms);
            audible = audible && rms > 0.02;            // 0.25 peak sine = 0.177 rms; nothing may mute it
            sane = sane && rms < 0.35;                  // and none may blow up (< +6 dB)
            std::printf ("       %-18s rms %.3f (%+.1f dB re 0.177)\n", q.nameEn, rms, 20.0 * std::log10 (rms / 0.17678));
        }
        check (inRange, "every preset value is inside its parameter range");
        check (unique, "preset names are unique");
        check (finite, "every preset renders finite output");
        check (audible, "no preset mutes the signal");
        check (sane, "no preset blows up the level");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
