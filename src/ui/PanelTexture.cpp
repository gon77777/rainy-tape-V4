#include "PanelTexture.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rainy
{
namespace
{
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float smooth (float e0, float e1, float x)
{
    const float t = clamp01 ((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

inline float hash2 (int x, int y)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffffu) / 65535.0f;
}

// Tiling value noise on a 256x256 lattice.
class Lattice
{
public:
    explicit Lattice (juce::int64 seed)
    {
        juce::Random r (seed);
        grid.resize (256 * 256);
        for (auto& v : grid) v = r.nextFloat();
    }

    float noise (float x, float y) const
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx);
        fy = fy * fy * (3.0f - 2.0f * fy);
        auto at = [this] (int ix, int iy) { return grid[(size_t) (((iy & 255) << 8) | (ix & 255))]; };
        const float a = at (xi, yi), b = at (xi + 1, yi), c = at (xi, yi + 1), d = at (xi + 1, yi + 1);
        return lerpf (lerpf (a, b, fx), lerpf (c, d, fx), fy);
    }

    float fbm (float x, float y, int octaves) const
    {
        float sum = 0.0f, amp = 0.5f, tot = 0.0f;
        for (int i = 0; i < octaves; ++i)
        {
            sum += amp * noise (x, y);
            tot += amp;
            x = x * 2.03f + 17.1f;
            y = y * 2.03f + 5.7f;
            amp *= 0.5f;
        }
        return sum / tot;
    }

private:
    std::vector<float> grid;
};

// A noise field pre-computed on a coarse grid and sampled bilinearly,
// which keeps full-resolution generation fast.
struct Field
{
    int w = 0, h = 0;
    float stepX = 1.0f, stepY = 1.0f;
    std::vector<float> d;

    template <typename Fn>
    void build (int W, int H, float sx, float sy, Fn fn)
    {
        stepX = sx; stepY = sy;
        w = (int) std::ceil ((float) W / sx) + 2;
        h = (int) std::ceil ((float) H / sy) + 2;
        d.resize ((size_t) (w * h));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                d[(size_t) (y * w + x)] = fn ((float) x * sx, (float) y * sy);
    }

    float at (float lx, float ly) const
    {
        const float fx = lx / stepX, fy = ly / stepY;
        const int x0 = std::clamp ((int) fx, 0, w - 2);
        const int y0 = std::clamp ((int) fy, 0, h - 2);
        const float tx = fx - (float) x0, ty = fy - (float) y0;
        const float* r0 = &d[(size_t) (y0 * w + x0)];
        const float* r1 = r0 + w;
        return lerpf (lerpf (r0[0], r0[1], tx), lerpf (r1[0], r1[1], tx), ty);
    }
};

// Signed distance to a rounded rectangle (negative inside).
inline float sdRoundRect (float px, float py, float cx, float cy, float hx, float hy, float r)
{
    const float qx = std::abs (px - cx) - hx + r;
    const float qy = std::abs (py - cy) - hy + r;
    const float ox = std::max (qx, 0.0f), oy = std::max (qy, 0.0f);
    return std::sqrt (ox * ox + oy * oy) + std::min (std::max (qx, qy), 0.0f) - r;
}
} // namespace

PanelImages makePanelImages (int W, int H, int S)
{
    const int pw = W * S, ph = H * S;

    Lattice lat (0x52414e59); // "RANY"

    Field mottle, wearF, patchF, toneF, speckF, streakF;
    mottle.build (W, H, 4.0f, 4.0f, [&] (float x, float y) { return lat.fbm (x / 70.0f, y / 70.0f, 3); });
    wearF.build  (W, H, 1.5f, 1.5f, [&] (float x, float y) { return lat.fbm (x / 26.0f + 40.0f, y / 26.0f + 9.0f, 4); });
    patchF.build (W, H, 4.0f, 4.0f, [&] (float x, float y) { return lat.fbm (x / 95.0f + 91.0f, y / 95.0f + 33.0f, 3); });
    toneF.build  (W, H, 1.5f, 1.5f, [&] (float x, float y) { return lat.fbm (x / 9.0f + 7.0f, y / 9.0f + 71.0f, 3); });
    speckF.build (W, H, 0.75f, 0.75f, [&] (float x, float y) { return lat.fbm (x / 2.6f + 13.0f, y / 2.6f + 57.0f, 2); });
    // Rain streaks: very narrow in x, very long in y.
    streakF.build (W, H, 0.5f, 6.0f, [&] (float x, float y) { return lat.noise (x * 0.42f + 200.0f, y * 0.011f + 50.0f); });

    PanelImages out;
    out.paint = juce::Image (juce::Image::RGB, pw, ph, false);
    out.wear  = juce::Image (juce::Image::ARGB, pw, ph, true);

    {
        juce::Image::BitmapData pb (out.paint, juce::Image::BitmapData::writeOnly);
        juce::Image::BitmapData wb (out.wear, juce::Image::BitmapData::writeOnly);

        const float fW = (float) W, fH = (float) H;
        const float frame = 10.0f;

        for (int py = 0; py < ph; ++py)
        {
            auto* prow = reinterpret_cast<juce::PixelRGB*> (pb.getLinePointer (py));
            auto* wrow = reinterpret_cast<juce::PixelARGB*> (wb.getLinePointer (py));
            const float ly = ((float) py + 0.5f) / (float) S;

            for (int px = 0; px < pw; ++px)
            {
                const float lx = ((float) px + 0.5f) / (float) S;

                const float sdOuter = sdRoundRect (lx, ly, fW * 0.5f, fH * 0.5f, fW * 0.5f, fH * 0.5f, 20.0f);
                const float sdInner = sdRoundRect (lx, ly, fW * 0.5f, fH * 0.5f, fW * 0.5f - frame, fH * 0.5f - frame, 12.0f);

                if (sdOuter > 0.0f)
                {
                    prow[px].setARGB (255, 20, 13, 9);
                    wrow[px] = juce::PixelARGB (0, 0, 0, 0);
                    continue;
                }

                const bool inFrame = sdInner > 0.0f;
                const float grain = (hash2 (px, py) - 0.5f) * 0.07f;
                const float mott = (mottle.at (lx, ly) - 0.5f) * 0.34f;

                float r, g, b;

                if (! inFrame)
                {
                    const float dx = (lx - fW * 0.5f) / (fW * 0.5f);
                    const float dy = (ly - fH * 0.5f) / (fH * 0.5f);
                    const float vig = 1.0f - 0.22f * (dx * dx * 0.7f + dy * dy);
                    const float topLight = 1.0f + 0.12f * (0.5f - ly / fH);
                    const float s = juce::jlimit (-1.0f, 1.0f, (streakF.at (lx, ly) - 0.5f) * 2.0f);
                    const float streak = s * 0.075f * (0.6f + 0.8f * mottle.at (lx + 300.0f, ly));
                    const float inset = 1.0f - 0.42f * smooth (-9.0f, 0.0f, sdInner);

                    const float L = vig * topLight * (1.0f + mott + streak + grain) * inset;
                    r = 120.0f * L * (1.0f + mott * 0.25f);
                    g = 79.0f * L;
                    b = 50.0f * L * (1.0f - mott * 0.2f);
                }
                else
                {
                    float L = 1.0f + mott * 0.8f + grain * 1.4f;
                    // Light catches the outer edge and the inner bevel.
                    L += 0.30f * (1.0f - smooth (0.0f, 2.4f, -sdOuter));
                    L += 0.22f * (1.0f - smooth (0.0f, 2.6f, sdInner));
                    r = 70.0f * L;
                    g = 48.0f * L;
                    b = 34.0f * L;
                }

                prow[px].setARGB (255, (juce::uint8) juce::jlimit (0, 255, (int) r),
                                       (juce::uint8) juce::jlimit (0, 255, (int) g),
                                       (juce::uint8) juce::jlimit (0, 255, (int) b));

                // ---- wear overlay -------------------------------------------------
                const float edgeDist = inFrame ? 0.0f : -sdInner;
                const float edgeBoost = clamp01 (1.0f - edgeDist / 34.0f);
                float score = (wearF.at (lx, ly) - 0.5f) * 3.2f + edgeBoost * edgeBoost * 0.55f - 0.64f
                              + (inFrame ? 0.22f : 0.0f)
                              + smooth (0.71f, 0.79f, patchF.at (lx, ly)) * 0.5f;

                const float chip = smooth (0.0f, 0.09f, score);
                const float core = smooth (0.10f, 0.45f, score);

                const float tone = clamp01 ((toneF.at (lx, ly) - 0.3f) * 2.5f);
                const float pit = 0.82f + 0.36f * hash2 (px / 2 + 11, py / 2 + 3);

                float cr = lerpf (150.0f, lerpf (92.0f, 156.0f, tone) * pit, core);
                float cg = lerpf (112.0f, lerpf (46.0f, 84.0f, tone) * pit, core);
                float cb = lerpf (78.0f,  lerpf (24.0f, 42.0f, tone) * pit, core);
                float alpha = chip;

                const float speck = smooth (0.64f, 0.70f, speckF.at (lx, ly)) * (0.2f + 0.8f * edgeBoost) * 0.8f;
                if (speck > alpha)
                {
                    cr = 120.0f * pit; cg = 52.0f * pit; cb = 24.0f * pit;
                    alpha = speck;
                }

                juce::PixelARGB p ((juce::uint8) juce::jlimit (0, 255, (int) (alpha * 255.0f)),
                                   (juce::uint8) juce::jlimit (0, 255, (int) cr),
                                   (juce::uint8) juce::jlimit (0, 255, (int) cg),
                                   (juce::uint8) juce::jlimit (0, 255, (int) cb));
                p.premultiply();
                wrow[px] = p;
            }
        }
    }

    // ---- scratches on the wear layer ------------------------------------------
    {
        juce::Graphics g (out.wear);
        g.addTransform (juce::AffineTransform::scale ((float) S));
        juce::Random rnd (0x5c4a7c);

        for (int i = 0; i < 110; ++i)
        {
            const float x = 16.0f + rnd.nextFloat() * (float) (W - 32);
            const float y = 16.0f + rnd.nextFloat() * (float) (H - 32);
            const float len = 6.0f + rnd.nextFloat() * rnd.nextFloat() * 70.0f;
            const float ang = rnd.nextFloat() < 0.7f
                                ? (rnd.nextFloat() - 0.5f) * 1.2f + (rnd.nextBool() ? 0.0f : juce::MathConstants<float>::pi)
                                : rnd.nextFloat() * juce::MathConstants<float>::twoPi;
            const bool dark = rnd.nextFloat() < 0.18f;
            g.setColour (dark ? juce::Colour (0xff1d1009).withAlpha (0.20f + rnd.nextFloat() * 0.2f)
                              : juce::Colour (0xffe6c9a0).withAlpha (0.07f + rnd.nextFloat() * 0.20f));
            g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.5f + rnd.nextFloat() * 0.6f);
        }
    }

    return out;
}
} // namespace rainy
