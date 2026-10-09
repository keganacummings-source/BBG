#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "Core/bg_analysis.h"

namespace BabyGirl
{
    namespace Theme
    {
        inline const juce::Colour back    { 0xff0b0c10 };
        inline const juce::Colour panel   { 0xff15161b };
        inline const juce::Colour panel2  { 0xff1c1d24 };
        inline const juce::Colour edge    { 0xff2f313a };
        inline const juce::Colour amber   { 0xfff59e0b };
        inline const juce::Colour cyan    { 0xff22d3ee };
        inline const juce::Colour green   { 0xff34d399 };
        inline const juce::Colour pink    { 0xfff472b6 };
        inline const juce::Colour violet  { 0xffa78bfa };
        inline const juce::Colour text    { 0xffe4e4e7 };
        inline const juce::Colour dim     { 0xff8b8d98 };
        inline juce::Colour family(int fam)
        {
            static const juce::Colour c[] = { juce::Colour(0xff94a3b8), juce::Colour(0xfff59e0b), juce::Colour(0xff34d399), juce::Colour(0xff22d3ee),
                                              juce::Colour(0xfffb7185), juce::Colour(0xff60a5fa), juce::Colour(0xffa78bfa), juce::Colour(0xfff472b6),
                                              juce::Colour(0xfffacc15), juce::Colour(0xff2dd4bf), juce::Colour(0xffc084fc), juce::Colour(0xfffb923c) };
            return c[juce::jlimit(0, 11, fam)];
        }
    }

    /** One analysis pass per display frame, shared by every visualiser. */
    struct AnalyzerHub
    {
        static constexpr int order = 12, N = 1 << order, bins = N / 2, scopeN = 2048;
        juce::dsp::FFT fft { order };
        juce::dsp::WindowingFunction<float> window { (size_t)N, juce::dsp::WindowingFunction<float>::hann };
        std::array<float, 2 * N> fftData {};
        std::array<float, N> L {}, R {};
        std::array<float, bins + 1> db {}, peak {};
        std::array<float, scopeN> scopeL {}, scopeR {};
        float levelL = -100.0f, levelR = -100.0f, levelHoldL = -100.0f, levelHoldR = -100.0f;
        double sampleRate = 48000.0;
        // user settings
        float smoothing = 0.55f, peakFallDbPerSec = 14.0f, slopeDb = 4.5f, floorDb = -96.0f;
        bool holdPeaks = true, frozen = false;
        int frameCounter = 0;

        AnalyzerHub() { db.fill(-120.0f); peak.fill(-120.0f); }

        void update(const bg::ScopeRing& ring, double sr, float dt)
        {
            sampleRate = sr; ++frameCounter;
            ring.latest(L.data(), R.data(), N);
            // scope + level data (always live, even when the spectrum is frozen)
            for (int i = 0; i < scopeN; ++i) { scopeL[(size_t)i] = L[(size_t)(N - scopeN + i)]; scopeR[(size_t)i] = R[(size_t)(N - scopeN + i)]; }
            float pl = 0, pr = 0;
            for (int i = N - 1024; i < N; ++i) { pl = std::max(pl, std::fabs(L[(size_t)i])); pr = std::max(pr, std::fabs(R[(size_t)i])); }
            const float dl = juce::Decibels::gainToDecibels(pl, -100.0f), dr = juce::Decibels::gainToDecibels(pr, -100.0f);
            levelL = dl > levelL ? dl : levelL + (dl - levelL) * 0.2f; levelR = dr > levelR ? dr : levelR + (dr - levelR) * 0.2f;
            levelHoldL = std::max(dl, levelHoldL - 20.0f * dt); levelHoldR = std::max(dr, levelHoldR - 20.0f * dt);
            if (frozen) return;

            for (int i = 0; i < N; ++i) fftData[(size_t)i] = 0.5f * (L[(size_t)i] + R[(size_t)i]);
            std::fill(fftData.begin() + N, fftData.end(), 0.0f);
            window.multiplyWithWindowingTable(fftData.data(), (size_t)N);
            fft.performFrequencyOnlyForwardTransform(fftData.data());
            const float norm = 1.0f / ((float)N * 0.25f);
            for (int i = 0; i <= bins; ++i)
            {
                const float d = juce::Decibels::gainToDecibels(fftData[(size_t)i] * norm, -140.0f);
                db[(size_t)i] = d > db[(size_t)i] ? db[(size_t)i] + (d - db[(size_t)i]) * 0.7f : db[(size_t)i] + (d - db[(size_t)i]) * (1.0f - smoothing);
                peak[(size_t)i] = holdPeaks ? std::max(db[(size_t)i], peak[(size_t)i] - peakFallDbPerSec * dt) : db[(size_t)i];
            }
        }

        float freqToBinF(float f) const { return f / (float)(sampleRate * 0.5) * (float)bins; }
        float tilt(float f) const { return slopeDb * std::log2(std::max(f, 20.0f) / 1000.0f); }
    };

    static inline float freqToX(float f, float w) { return w * std::log(f / 20.0f) / std::log(1000.0f); }
    static inline float xToFreq(float x, float w) { return 20.0f * std::pow(1000.0f, x / w); }

    // ------------------------------------------------------------------ spectrum
    class SpectrumView : public juce::Component
    {
    public:
        explicit SpectrumView(AnalyzerHub& h, bool compact = false) : hub(h), mini(compact) { setOpaque(true); }
        void paint(juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const float w = b.getWidth(), hgt = b.getHeight();
            g.fillAll(Theme::back);
            g.setGradientFill(juce::ColourGradient::vertical(juce::Colour(0xff12131a), 0.0f, juce::Colour(0xff0a0b0f), hgt));
            g.fillRect(b);
            const float top = mini ? 2.0f : 18.0f, bottom = hgt - (mini ? 2.0f : 16.0f), range = bottom - top;
            const float dbTop = 6.0f, dbBot = hub.floorDb;
            auto yOf = [&](float d) { return top + (dbTop - juce::jlimit(dbBot, dbTop, d)) / (dbTop - dbBot) * range; };

            if (!mini)
            {
                g.setFont(juce::Font(10.0f));
                for (float f : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
                {
                    const float x = freqToX(f, w); g.setColour(juce::Colours::white.withAlpha(0.06f)); g.drawVerticalLine((int)x, top, bottom);
                    g.setColour(Theme::dim); g.drawText(f >= 1000.f ? juce::String(f / 1000.f) + "k" : juce::String((int)f), (int)x - 16, (int)bottom + 2, 32, 12, juce::Justification::centred);
                }
                for (float d = 0.0f; d >= dbBot; d -= 12.0f)
                {
                    const float y = yOf(d); g.setColour(juce::Colours::white.withAlpha(d == 0.0f ? 0.14f : 0.06f)); g.drawHorizontalLine((int)y, 0.0f, w);
                    g.setColour(Theme::dim); g.drawText(juce::String((int)d), 2, (int)y - 6, 28, 12, juce::Justification::left);
                }
                g.setColour(Theme::dim); g.drawText("SPECTRUM  4096-pt FFT  " + juce::String(hub.slopeDb, 1) + " dB/oct", 34, 2, 260, 14, juce::Justification::left);
            }

            const int cols = juce::jmax(2, (int)(w / (mini ? 2.0f : 1.5f)));
            juce::Path fill, line, pk;
            for (int c = 0; c < cols; ++c)
            {
                const float x = w * (float)c / (float)(cols - 1);
                const float f0 = xToFreq(w * (float)c / (float)cols, w), f1 = xToFreq(w * (float)(c + 1) / (float)cols, w);
                const float b0 = hub.freqToBinF(f0), b1 = std::max(hub.freqToBinF(f1), b0 + 1.0e-3f);
                float d, p;
                if (b1 - b0 < 1.0f) { const int i0 = juce::jlimit(0, AnalyzerHub::bins - 1, (int)b0); const float fr = b0 - (float)i0; d = hub.db[(size_t)i0] * (1 - fr) + hub.db[(size_t)i0 + 1] * fr; p = hub.peak[(size_t)i0] * (1 - fr) + hub.peak[(size_t)i0 + 1] * fr; }
                else { d = -200.0f; p = -200.0f; for (int i = (int)b0; i <= juce::jmin((int)b1, AnalyzerHub::bins); ++i) { d = std::max(d, hub.db[(size_t)i]); p = std::max(p, hub.peak[(size_t)i]); } }
                const float t = hub.tilt(std::sqrt(f0 * f1));
                const float y = yOf(d + t), yp = yOf(p + t);
                if (c == 0) { fill.startNewSubPath(x, bottom); fill.lineTo(x, y); line.startNewSubPath(x, y); pk.startNewSubPath(x, yp); }
                else { fill.lineTo(x, y); line.lineTo(x, y); pk.lineTo(x, yp); }
            }
            fill.lineTo(w, bottom); fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient(Theme::amber.withAlpha(0.55f), 0.0f, top, Theme::pink.withAlpha(0.04f), 0.0f, bottom, false));
            g.fillPath(fill);
            if (hub.holdPeaks && !mini) { g.setColour(juce::Colours::white.withAlpha(0.35f)); g.strokePath(pk, juce::PathStrokeType(1.0f)); }
            g.setColour(Theme::amber.brighter(0.3f).withAlpha(0.35f)); g.strokePath(line, juce::PathStrokeType(mini ? 3.0f : 4.5f));
            g.setColour(Theme::amber.brighter(0.6f)); g.strokePath(line, juce::PathStrokeType(mini ? 1.0f : 1.6f));
            g.setColour(Theme::edge); g.drawRect(b, 1.0f);
        }
    private:
        AnalyzerHub& hub; bool mini;
    };

    // ------------------------------------------------------------------ spectrogram
    class SpectrogramView : public juce::Component
    {
    public:
        explicit SpectrogramView(AnalyzerHub& h) : hub(h)
        {
            setOpaque(true);
            juce::ColourGradient cg(juce::Colour(0xff000004), 0.0f, 0.0f, juce::Colour(0xfffcffa4), 1.0f, 0.0f, false);   // inferno ramp
            cg.addColour(0.18, juce::Colour(0xff1b0c41)); cg.addColour(0.38, juce::Colour(0xff6a176e)); cg.addColour(0.58, juce::Colour(0xffbb3754));
            cg.addColour(0.78, juce::Colour(0xfff98e09));
            for (int i = 0; i < 256; ++i) lut[(size_t)i] = cg.getColourAtPosition((double)i / 255.0);
        }
        void resized() override { img = juce::Image(juce::Image::ARGB, juce::jmax(2, getWidth()), juce::jmax(2, getHeight()), true); juce::Graphics(img).fillAll(juce::Colours::black); x = 0; }
        void tick()
        {
            if (!img.isValid() || hub.frozen) return;
            juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
            const int H = img.getHeight(), W = img.getWidth();
            for (int y = 0; y < H; ++y)
            {
                const float f = xToFreq((float)(H - 1 - y) / (float)H * (float)H, (float)H);
                const float bf = hub.freqToBinF(f); const int i0 = juce::jlimit(0, AnalyzerHub::bins - 1, (int)bf);
                const float d = hub.db[(size_t)i0] * (1 - (bf - (float)i0)) + hub.db[(size_t)i0 + 1] * (bf - (float)i0) + hub.tilt(f);
                const float t = juce::jlimit(0.0f, 1.0f, (d - hub.floorDb) / (0.0f - hub.floorDb));
                bd.setPixelColour(x, y, lut[(size_t)(t * 255.0f)]);
            }
            x = (x + 1) % W;
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black);
            if (img.isValid())
            {
                const int W = img.getWidth(), H = img.getHeight(), right = W - x;
                g.drawImage(img, 0, 0, right, H, x, 0, right, H);
                if (x > 0) g.drawImage(img, right, 0, x, H, 0, 0, x, H);
            }
            g.setFont(juce::Font(10.0f)); g.setColour(juce::Colours::white.withAlpha(0.55f));
            for (float f : { 100.f, 1000.f, 10000.f }) { const float y = (float)getHeight() * (1.0f - freqToX(f, (float)getHeight()) / (float)getHeight()); g.drawHorizontalLine((int)y, 0.0f, 6.0f); g.drawText(f >= 1000.f ? juce::String((int)(f / 1000.f)) + "k" : juce::String((int)f), 8, (int)y - 6, 30, 12, juce::Justification::left); }
            g.setColour(Theme::dim); g.drawText("SPECTROGRAM", getWidth() - 100, 2, 96, 12, juce::Justification::right);
            g.setColour(Theme::edge); g.drawRect(getLocalBounds(), 1);
        }
    private:
        AnalyzerHub& hub; juce::Image img; int x = 0; std::array<juce::Colour, 256> lut;
    };

    // ------------------------------------------------------------------ oscilloscope
    class ScopeView : public juce::Component
    {
    public:
        explicit ScopeView(AnalyzerHub& h) : hub(h) { setOpaque(true); }
        float zoom = 0.5f;   // 0..1 -> 256..1536 samples shown
        void paint(juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat(); const float w = b.getWidth(), hh = b.getHeight();
            g.fillAll(juce::Colour(0xff07090b));
            g.setColour(juce::Colour(0xff0f2a1a).withAlpha(0.8f));
            for (int i = 1; i < 8; ++i) g.drawVerticalLine((int)(w * i / 8.0f), 0.0f, hh);
            for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int)(hh * i / 4.0f), 0.0f, w);
            g.setColour(juce::Colour(0xff1d4a2e)); g.drawHorizontalLine((int)(hh * 0.5f), 0.0f, w);
            const int len = 256 + (int)(zoom * 1280.0f);
            int start = 0;                                    // rising zero-crossing trigger for a stable picture
            for (int i = 1; i < AnalyzerHub::scopeN - len - 1; ++i) if (hub.scopeL[(size_t)i - 1] < 0.0f && hub.scopeL[(size_t)i] >= 0.0f) { start = i; break; }
            auto trace = [&](const std::array<float, AnalyzerHub::scopeN>& s, juce::Colour c)
            {
                juce::Path p;
                for (int i = 0; i < len; ++i)
                {
                    const float x = w * (float)i / (float)(len - 1), y = hh * 0.5f - juce::jlimit(-1.2f, 1.2f, s[(size_t)(start + i)]) * hh * 0.42f;
                    if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
                }
                g.setColour(c.withAlpha(0.18f)); g.strokePath(p, juce::PathStrokeType(5.0f));
                g.setColour(c.withAlpha(0.35f)); g.strokePath(p, juce::PathStrokeType(2.5f));
                g.setColour(c.brighter(0.5f)); g.strokePath(p, juce::PathStrokeType(1.1f));
            };
            trace(hub.scopeR, Theme::cyan); trace(hub.scopeL, Theme::green);
            g.setFont(juce::Font(10.0f)); g.setColour(Theme::dim);
            g.drawText("SCOPE  L", 6, 2, 70, 12, juce::Justification::left); g.setColour(Theme::cyan); g.drawText("R", 70, 2, 12, 12, juce::Justification::left);
            g.setColour(Theme::edge); g.drawRect(getLocalBounds(), 1);
        }
    private:
        AnalyzerHub& hub;
    };

    // ------------------------------------------------------------------ goniometer (vectorscope, phosphor persistence)
    class GonioView : public juce::Component
    {
    public:
        explicit GonioView(AnalyzerHub& h) : hub(h) { setOpaque(true); }
        void resized() override { const int s = juce::jmax(2, juce::jmin(getWidth(), getHeight())); phosphor = juce::Image(juce::Image::ARGB, s, s, true); juce::Graphics(phosphor).fillAll(juce::Colours::black); }
        void tick()
        {
            if (!phosphor.isValid()) return;
            juce::Graphics g(phosphor);
            g.setColour(juce::Colours::black.withAlpha(0.16f)); g.fillAll();
            const float s = (float)phosphor.getWidth(), cx = s * 0.5f, cy = s * 0.5f, sc = s * 0.46f;
            juce::Path p; bool first = true;
            for (int i = AnalyzerHub::scopeN - 640; i < AnalyzerHub::scopeN; ++i)
            {
                const float l = hub.scopeL[(size_t)i], r = hub.scopeR[(size_t)i];
                const float x = cx + (r - l) * 0.7071f * sc, y = cy - (l + r) * 0.7071f * sc;
                if (first) { p.startNewSubPath(x, y); first = false; } else p.lineTo(x, y);
            }
            g.setColour(Theme::green.withAlpha(0.28f)); g.strokePath(p, juce::PathStrokeType(2.6f));
            g.setColour(Theme::green.brighter(0.7f).withAlpha(0.8f)); g.strokePath(p, juce::PathStrokeType(0.9f));
        }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black);
            const int s = juce::jmin(getWidth(), getHeight()); const int ox = (getWidth() - s) / 2, oy = (getHeight() - s) / 2;
            if (phosphor.isValid()) g.drawImageAt(phosphor, ox, oy);
            const float cx = (float)ox + s * 0.5f, cy = (float)oy + s * 0.5f, r = s * 0.46f;
            g.setColour(juce::Colour(0xff1d4a2e)); g.drawEllipse(cx - r, cy - r, 2 * r, 2 * r, 1.0f);
            g.drawLine(cx - r, cy, cx + r, cy); g.drawLine(cx, cy - r, cx, cy + r);
            g.drawLine(cx - r * 0.7071f, cy - r * 0.7071f, cx + r * 0.7071f, cy + r * 0.7071f, 0.6f); g.drawLine(cx - r * 0.7071f, cy + r * 0.7071f, cx + r * 0.7071f, cy - r * 0.7071f, 0.6f);
            g.setFont(juce::Font(10.0f)); g.setColour(Theme::dim);
            g.drawText("M", (int)cx - 6, oy + 2, 12, 12, juce::Justification::centred); g.drawText("L", (int)(cx - r * 0.7071f) - 14, (int)(cy - r * 0.7071f) - 6, 12, 12, juce::Justification::centred);
            g.drawText("R", (int)(cx + r * 0.7071f) + 2, (int)(cy - r * 0.7071f) - 6, 12, 12, juce::Justification::centred); g.drawText("S", ox + s - 14, (int)cy - 6, 12, 12, juce::Justification::centred);
            g.setColour(Theme::edge); g.drawRect(getLocalBounds(), 1);
        }
    private:
        AnalyzerHub& hub; juce::Image phosphor;
    };

    // ------------------------------------------------------------------ meters
    class MeterView : public juce::Component
    {
    public:
        MeterView(AnalyzerHub& h, bg::LoudnessMeter& l) : hub(h), loud(l) { setOpaque(true); }
        void paint(juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat(); g.fillAll(Theme::panel);
            const float M = loud.momentary.load(), S = loud.shortTerm.load(), I = loud.integrated.load(), TP = loud.truePeakDb.load(), C = loud.correlation.load();
            auto num = [&](const juce::String& lab, float v, int x, juce::Colour c)
            {
                g.setFont(juce::Font(9.5f, juce::Font::bold)); g.setColour(Theme::dim); g.drawText(lab, x, 4, 60, 11, juce::Justification::left);
                g.setFont(juce::Font(20.0f, juce::Font::bold)); g.setColour(c); g.drawText(v <= -99.0f ? "-inf" : juce::String(v, 1), x, 15, 66, 24, juce::Justification::left);
            };
            num("MOMENTARY", M, 8, Theme::amber); num("SHORT-TERM", S, 80, Theme::amber); num("INTEGRATED", I, 152, Theme::green); num("TRUE PEAK", TP, 224, TP > -1.0f ? Theme::pink : Theme::cyan);
            g.setFont(juce::Font(9.0f)); g.setColour(Theme::dim); g.drawText("LUFS (BS.1770)  |  dBTP", 296, 6, 130, 11, juce::Justification::left);
            // L/R bars
            const float bx = 8.0f, bw = b.getWidth() - 16.0f; float y = 46.0f;
            auto bar = [&](float db, float hold, juce::Colour c, const juce::String& lab)
            {
                g.setColour(Theme::back); g.fillRoundedRectangle(bx + 14, y, bw - 14, 9.0f, 3.0f);
                const float t = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f), th = juce::jlimit(0.0f, 1.0f, (hold + 60.0f) / 60.0f);
                g.setGradientFill(juce::ColourGradient(Theme::green, bx + 14, 0, Theme::pink, bx + bw, 0, false)); g.fillRoundedRectangle(bx + 14, y, (bw - 14) * t, 9.0f, 3.0f);
                g.setColour(juce::Colours::white); g.fillRect(bx + 14 + (bw - 14) * th - 1.0f, y, 2.0f, 9.0f);
                g.setFont(juce::Font(9.0f, juce::Font::bold)); g.setColour(c); g.drawText(lab, (int)bx, (int)y - 1, 12, 11, juce::Justification::left); y += 13.0f;
            };
            bar(hub.levelL, hub.levelHoldL, Theme::green, "L"); bar(hub.levelR, hub.levelHoldR, Theme::cyan, "R");
            // correlation
            g.setColour(Theme::back); g.fillRoundedRectangle(bx + 14, y + 2, bw - 14, 9.0f, 3.0f);
            const float mid = bx + 14 + (bw - 14) * 0.5f, px = bx + 14 + (bw - 14) * (C * 0.5f + 0.5f);
            g.setColour(C < 0.0f ? Theme::pink : Theme::green); g.fillRect(juce::jmin(mid, px), y + 2, std::abs(px - mid), 9.0f);
            g.setColour(juce::Colours::white); g.fillRect(px - 1.0f, y + 1, 2.0f, 11.0f);
            g.setFont(juce::Font(9.0f, juce::Font::bold)); g.setColour(Theme::dim); g.drawText("COR", (int)bx - 2, (int)y + 1, 16, 11, juce::Justification::left);
            g.setColour(Theme::edge); g.drawRect(b, 1.0f);
        }
    private:
        AnalyzerHub& hub; bg::LoudnessMeter& loud;
    };
}
